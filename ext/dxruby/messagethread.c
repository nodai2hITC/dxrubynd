#define WINVER 0x0500                                  /* �o�[�W������` Windows2000�ȏ� */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#include "ruby/encoding.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1

#include "dxruby.h"
#include "messagethread.h"
#include "font.h"
#include "imm.h"

static HANDLE hMessageThread;
static DWORD MessageThreadID;
static HANDLE hEventMainThreadStart;
int MainThreadError = 0;

static int InitWindow( void );
static int InitDXGraphics( void );
static void DXRelease();

#ifdef DXRUBY15
int ime_str_length;
int ime_str_length_old;
WCHAR *ime_buf = NULL;
WCHAR *ime_buf_old = NULL;
CRITICAL_SECTION ime_cs;
int ime_compositing;

int ime_vk_push_str_length;
char *ime_vk_push_buf = NULL;
char *ime_vk_push_buf_old = NULL;

int ime_vk_release_str_length;
char *ime_vk_release_buf = NULL;
char *ime_vk_release_buf_old = NULL;

WCHAR *ime_composition_str = NULL;
WCHAR *ime_composition_str_old = NULL;
int ime_composition_attr_size;
char *ime_composition_attr = 0;
char *ime_composition_attr_old = 0;
LPCANDIDATELIST ime_canlist = NULL;
LPCANDIDATELIST ime_canlist_old = NULL;
int ime_cursor_pos;

HIMC default_imc = NULL;
#endif

char *ERR_MESSAGE[ERR_MAX] =
{
    "ok",
    "out of memory",
    "Internal Error",
    "not changed screen mode",
    "failed create window",
    "DirectX Graphics initialize error",
    "DirectInput initialize error"
};

void DXRuby_raise( int errorcode, char *msg )
{
    char buf[1024];
    buf[0] = '\0';

    strcat( buf, ERR_MESSAGE[errorcode] );
    strcat( buf, " - " );
    strcat( buf, msg );

    /* buf�̒��g�����b�Z�[�W�Ƃ���eDXRubyError��O�𓊂��� */
    rb_funcall( rb_cObject, rb_intern( "raise" ), 2, eDXRubyError, rb_funcall( rb_str_new2( buf ), rb_intern( "force_encoding" ), 1, rb_str_new2( sys_encode ) ) );
}

/*--------------------------------------------------------------------
  �i�����֐��j�X�N���[���T�C�Y�ύX
 ---------------------------------------------------------------------*/
static int DDChangeSize( void )
{
    D3DVIEWPORT9 vp;
    HRESULT hr;
    int i;
    D3DDISPLAYMODE d3ddm;

    g_D3DPP.BackBufferWidth    = g_WindowInfo.width;
    g_D3DPP.BackBufferHeight   = g_WindowInfo.height;

    if( !g_WindowInfo.windowed )
    {
        /* �t���X�N���[�����i32bitColor�Œ�/���t���b�V�����[�g�͂���΂���A������Έ�ԍ�������) */
        int count , max, set_refreshrate;

        max = g_pD3D->lpVtbl->GetAdapterModeCount( g_pD3D, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8 );
        set_refreshrate = 0;
        g_sync = 0;
        g_D3DPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        for( count = 0 ; count < max ; count++ )
        {
            g_pD3D->lpVtbl->EnumAdapterModes( g_pD3D, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8 , count , &d3ddm);
            /* �𑜓x��������疳�� */
            if( g_WindowInfo.width != d3ddm.Width || g_WindowInfo.height != d3ddm.Height )
            {
                continue;
            }

            /* fps�w�肪����A�������̂��������炻��Ɍ��� */
            if( g_WindowInfo.fps != 0 && d3ddm.RefreshRate == g_WindowInfo.fps )
            {
                set_refreshrate = d3ddm.RefreshRate;
                g_sync = 1; // �����������[�h
                g_D3DPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
                break;
            }

            /* �����ꍇ�ɂ͈�ԑ傫���z�ɂ��� */
            if( d3ddm.RefreshRate > set_refreshrate )
            {
                set_refreshrate = d3ddm.RefreshRate;
            }
        }

        g_D3DPP.BackBufferFormat           = D3DFMT_X8R8G8B8;
        g_D3DPP.Windowed                   = FALSE;
        g_D3DPP.FullScreen_RefreshRateInHz = set_refreshrate;
    }
    else
    {
        /* �E�B���h�E���[�h�� */

        /* ���݂̃��[�h���擾 */
        g_pD3D->lpVtbl->GetAdapterDisplayMode( g_pD3D, D3DADAPTER_DEFAULT , &d3ddm );

        g_sync = 0; // �����񓯊����[�h
        g_D3DPP.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
        g_D3DPP.BackBufferFormat           = D3DFMT_UNKNOWN;
        g_D3DPP.Windowed                   = TRUE;
        g_D3DPP.FullScreen_RefreshRateInHz = 0;
    }

    /* D3DXSprite�����X�g������ */
    if( g_pD3DXSprite )
    {
        g_pD3DXSprite->lpVtbl->OnLostDevice( g_pD3DXSprite );
    }

    /* ���C�������_�[�^�[�Q�b�g�̉�� */
    {
        struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
        RELEASE( rt->surface );
    }

    /* ���[�U�[�����_�[�^�[�Q�b�g�̉�� */
//    ppD3DTexture = (LPDIRECT3DTEXTURE9 *)alloca( g_RenderTargetList.count * sizeof(LPDIRECT3DTEXTURE9 *) );
    for( i = 0; i < g_RenderTargetList.count; i++ )
    {
        struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
        if( g_RenderTargetList.pointer[i] )
        {
//        ppD3DTexture[i] = to_image( rt );
            RELEASE( rt->surface );
            RELEASE( rt->texture->pD3DTexture );
        }
    }

    /* �V�F�[�_�̃��X�g */
    for( i = 0; i < g_ShaderCoreList.count; i++ )
    {
        struct DXRubyShaderCore *core = (struct DXRubyShaderCore *)g_ShaderCoreList.pointer[i];
        core->pD3DXEffect->lpVtbl->OnLostDevice( core->pD3DXEffect );
    }

    /* �ݒ�ύX */
    hr = g_pD3DDevice->lpVtbl->Reset( g_pD3DDevice, &g_D3DPP );
    if( FAILED( hr ) ) return 1;

    /* ���C�������_�[�^�[�Q�b�g�̕��� */
    {
        struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
        g_pD3DDevice->lpVtbl->GetRenderTarget( g_pD3DDevice, 0, &rt->surface );
    }

    /* ���[�U�[�����_�[�^�[�Q�b�g�̕��� */
    for( i = 0; i < g_RenderTargetList.count; i++ )
    {
        struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
        if( g_RenderTargetList.pointer[i] )
        {
            /* �e�N�X�`���I�u�W�F�N�g���쐬���� */
            hr = D3DXCreateTexture( g_pD3DDevice, (UINT)rt->texture->width, (UINT)rt->texture->height,
                                    1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                    &rt->texture->pD3DTexture);
            if( FAILED( hr ) ) return 2;

            hr = rt->texture->pD3DTexture->lpVtbl->GetSurfaceLevel( rt->texture->pD3DTexture, 0, &rt->surface );
            if( FAILED( hr ) ) return 3;
        }
    }

    /* �V�F�[�_�̕��A */
    for( i = 0; i < g_ShaderCoreList.count; i++ )
    {
        struct DXRubyShaderCore *core = (struct DXRubyShaderCore *)g_ShaderCoreList.pointer[i];
        core->pD3DXEffect->lpVtbl->OnResetDevice( core->pD3DXEffect );
    }

    /* D3DXSprite���A */
    if( g_pD3DXSprite )
    {
        g_pD3DXSprite->lpVtbl->OnResetDevice( g_pD3DXSprite );
    }

    return 0;
}

/*--------------------------------------------------------------------
   �E�B���h�E�̐����Ə�����
 ---------------------------------------------------------------------*/
static int ChangeSize()
{
    HRESULT hr;
    RECT rect;
    VALUE vrender_target;
    struct DXRubyRenderTarget *rt;
    int ret;

    /* �E�B���h�E�̃T�C�Y�ݒ� */
    rect.top     = 0;
    rect.left    = 0;
    rect.right   = (LONG)(g_WindowInfo.width * g_WindowInfo.scale);
    rect.bottom  = (LONG)(g_WindowInfo.height * g_WindowInfo.scale);

    /* �E�B���h�E�̃T�C�Y�C�� */
    if( !g_WindowInfo.windowed )
    {   /* �t���X�N���[�� */
        SetWindowLong( g_hWnd, GWL_STYLE, WS_POPUP );
        SetWindowPos( g_hWnd, HWND_TOP, 0, 0, g_WindowInfo.width, g_WindowInfo.height, 0);
    }
    else
    {   /* �E�B���h�E���[�h */
        SetWindowLong( g_hWnd, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
        AdjustWindowRect( &rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE );

        /* ���ƍ����v�Z */
        rect.right   = rect.right - rect.left;
        rect.bottom  = rect.bottom - rect.top;

        /* �E�B���h�E�ړ�/�T�C�Y�ݒ� */
        if( g_WindowInfo.x == CW_USEDEFAULT )
        {   /* �ʒu���f�t�H���g�̏ꍇ */
            SetWindowPos( g_hWnd, HWND_NOTOPMOST , 0, 0, rect.right, rect.bottom, SWP_NOMOVE);
        }
        else
        {   /* �ʒu�w��̏ꍇ */
            SetWindowPos( g_hWnd, HWND_NOTOPMOST, g_WindowInfo.x, g_WindowInfo.y, rect.right, rect.bottom, 0 );
        }
    }

    /* DirectX�̃X�N���[���T�C�Y�ύX */
    ret = DDChangeSize();
    if( ret != 0 )
    {
        return ret;
    }

    /* �E�B���h�E�\�� */
    ShowWindow( g_hWnd, SW_SHOWNORMAL );
    InvalidateRect( NULL, NULL, TRUE );
    UpdateWindow( g_hWnd );

    {
        struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );

        /* �V�[���̃N���A */
        g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
        g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                     D3DCOLOR_XRGB( rt->r, rt->g, rt->b ), 0, 0 );
        g_pD3DDevice->lpVtbl->Present( g_pD3DDevice, NULL, NULL, NULL, NULL );

        rt->width = g_WindowInfo.width;
        rt->height = g_WindowInfo.height;
    }

    g_WindowInfo.created = Qtrue;

    SetCursor( LoadCursor( NULL, IDC_ARROW ) );

    return 0;
}


static int reset( void )
{
    HRESULT hr;
    hr = g_pD3DDevice->lpVtbl->TestCooperativeLevel( g_pD3DDevice );
    if( hr == D3DERR_DEVICENOTRESET ) /* �f�o�C�X�̓��X�g��Ԃł��邪���Z�b�g�\�ł��� */
    {
        int i;
        /* �����֗������̓f�o�C�X�����Z�b�g�\��Ԃł��� */
        /* D3DXSprite�����X�g������ */
        if( g_pD3DXSprite )
        {
            g_pD3DXSprite->lpVtbl->OnLostDevice( g_pD3DXSprite );
        }

        /* �����_�[�^�[�Q�b�g�̉�� */
        {
            struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
            RELEASE( rt->surface );
        }
        for( i = 0; i < g_RenderTargetList.count; i++ )
        {
            struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
            if( g_RenderTargetList.pointer[i] )
            {
                RELEASE( rt->surface );
                RELEASE( rt->texture->pD3DTexture );
            }
        }

        /* �V�F�[�_�̃��X�g */
        for( i = 0; i < g_ShaderCoreList.count; i++ )
        {
            struct DXRubyShaderCore *core = (struct DXRubyShaderCore *)g_ShaderCoreList.pointer[i];
            core->pD3DXEffect->lpVtbl->OnLostDevice( core->pD3DXEffect );
        }

        hr = g_pD3DDevice->lpVtbl->Reset( g_pD3DDevice, &g_D3DPP ); /* ���������݂� */
        if( FAILED( hr ) )
        {
            if( hr == D3DERR_DEVICELOST )
            {
                return 5; /* �܂����X�g���� */
            }
            if( hr == D3DERR_INVALIDCALL )
            {
                return 10;
            }
            return 1;
        }

        /* �����_�[�^�[�Q�b�g�̕��� */
        {
            struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
            g_pD3DDevice->lpVtbl->GetRenderTarget( g_pD3DDevice, 0, &rt->surface );
        }
        for( i = 0; i < g_RenderTargetList.count; i++ )
        {
            struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
            if( g_RenderTargetList.pointer[i] )
            {
                /* �e�N�X�`���I�u�W�F�N�g���쐬���� */
                hr = D3DXCreateTexture( g_pD3DDevice, (UINT)rt->texture->width, (UINT)rt->texture->height,
                                        1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                        &rt->texture->pD3DTexture);
                if( FAILED( hr ) ) return 2;

                hr = rt->texture->pD3DTexture->lpVtbl->GetSurfaceLevel( rt->texture->pD3DTexture, 0, &rt->surface );
                if( FAILED( hr ) ) return 3;
            }
        }

        /* �V�F�[�_�̕��A */
        for( i = 0; i < g_ShaderCoreList.count; i++ )
        {
            struct DXRubyShaderCore *core = (struct DXRubyShaderCore *)g_ShaderCoreList.pointer[i];
            core->pD3DXEffect->lpVtbl->OnResetDevice( core->pD3DXEffect );
        }

        if( g_pD3DXSprite ) /* D3DXSprite���A */
        {
            g_pD3DXSprite->lpVtbl->OnResetDevice( g_pD3DXSprite );
        }
    }
    else if( hr == D3DERR_DEVICELOST )  /* �f�o�C�X�̓��X�g��Ԃł��� */
    {
        return 5; /* ���̋@��� */
    }
    else /* DirectX�̓����G���[ */
    {
        return 4;
    }

    return 0;
}


/*--------------------------------------------------------------------
   �X���b�h���C��
 ---------------------------------------------------------------------*/
static DWORD WINAPI MessageThreadProc( LPVOID lpParameter )
{
    MSG msg;
    msg.message = WM_NULL;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );

    /* �E�B���h�E�쐬(���̎��_�ł͔�\��) */
    InitWindow();
    if( MainThreadError != 0 )
    {
        SetEvent( hEventMainThreadStart );
        ExitThread( 0 );
    }

    /* DirectX Graphics�̏����� */
    InitDXGraphics();
    if( MainThreadError != 0 )
    {
        SetEvent( hEventMainThreadStart );
        ExitThread( 0 );
    }

    /* DirectInput������ */
    InitDirectInput();
    if( MainThreadError != 0 )
    {
        SetEvent( hEventMainThreadStart );
        ExitThread( 0 );
    }

    /* ���������I��������Ƃ̒ʒm */
    SetEvent( hEventMainThreadStart );

    /* ���b�Z�[�W���[�v */
    /* WM_QUIT���͂��܂Ń��b�Z�[�W�������������� */
    while( msg.message != WM_QUIT )
    {
        if( GetMessage( &msg, 0, 0, 0 ) != 0)
//        if( PeekMessage( &msg, g_hWnd, 0, 0, PM_REMOVE ) != 0)
        {
            /* ���b�Z�[�W�����鎞 */
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
//        g_byMouseState_L_buf = GetKeyState( VK_LBUTTON );
//        g_byMouseState_M_buf = GetKeyState( VK_MBUTTON );
//        g_byMouseState_R_buf = GetKeyState( VK_RBUTTON );
    }

    DXRelease();

    CoUninitialize();

    // �X���b�h�I��
    ExitThread( 0 );
}

/*--------------------------------------------------------------------
   �i�����֐��j�E�B���h�E�v���V�[�W��
 ---------------------------------------------------------------------*/
LRESULT CALLBACK MessageThreadWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    RECT rect;
    VALUE temp;

    switch( msg )
    {
    case WM_CREATE:
        break;

    case WM_ACTIVATE:
        /* �E�B���h�E�A�N�e�B�u�^��A�N�e�B�u�� */
        g_WindowInfo.active = (LOWORD(wParam) != 0);
        break;

    case WM_CLOSE:
        g_WindowInfo.requestclose = 1;
        return 0;

    case WM_DESTROY:
        /* �E�B���h�E�j�� */
        PostQuitMessage( 0 );
        g_hWnd = NULL;
        return 0;

    case WM_MOUSEWHEEL:
        g_WindowInfo.mousewheelpos += (short)HIWORD(wParam);
        break;

    case WM_LBUTTONDOWN:
        g_byMouseState_L_buf = 0x80;
        SetCapture(g_hWnd);
        break;
    case WM_LBUTTONUP:
        g_byMouseState_L_buf = 0x00;
        ReleaseCapture();
        break;
    case WM_MBUTTONDOWN:
        g_byMouseState_M_buf = 0x80;
        SetCapture(g_hWnd);
        break;
    case WM_MBUTTONUP:
        g_byMouseState_M_buf = 0x00;
        ReleaseCapture();
        break;
    case WM_RBUTTONDOWN:
        g_byMouseState_R_buf = 0x80;
        SetCapture(g_hWnd);
        break;
    case WM_RBUTTONUP:
        g_byMouseState_R_buf = 0x00;
        ReleaseCapture();
        break;

#ifdef DXRUBY15
    case WM_KEYDOWN:
        EnterCriticalSection( &ime_cs );
        if( ime_vk_push_str_length+1 >= IME_VK_BUF_SIZE-1 ) /* �o�b�t�@�I�[�o�[������N���A�����Ⴄ */
        {
            ime_vk_push_str_length = 0;
        }
        ime_vk_push_buf[ime_vk_push_str_length++] = (char)wParam;
        LeaveCriticalSection( &ime_cs );
        return 0;

    case WM_KEYUP:
        EnterCriticalSection( &ime_cs );
        if( ime_vk_release_str_length+1 >= IME_VK_BUF_SIZE-1 ) /* �o�b�t�@�I�[�o�[������N���A�����Ⴄ */
        {
            ime_vk_release_str_length = 0;
        }
        ime_vk_release_buf[ime_vk_release_str_length++] = (char)wParam;
        LeaveCriticalSection( &ime_cs );
        return 0;

    case WM_CHAR:
        if( wParam < 32 )
        {
            return 0;
        }

        EnterCriticalSection( &ime_cs );
        if( ime_str_length+1 >= IME_BUF_SIZE-1 ) /* �o�b�t�@�I�[�o�[������N���A�����Ⴄ */
        {
            ime_str_length = 0;
        }
        MultiByteToWideChar( CP_ACP, 0, (char *)&wParam, 1, (LPWSTR)&ime_buf[ime_str_length], 2 );
        ime_str_length += 1;
        ime_buf[ime_str_length] = 0;
        LeaveCriticalSection( &ime_cs );
        return 0;

    case WM_IME_CHAR:
        if( ((char)(wParam >> 8) & 0xff) != 0 )
        {
            EnterCriticalSection( &ime_cs );
            if( ime_str_length+1 >= IME_BUF_SIZE-1 ) /* �o�b�t�@�I�[�o�[������N���A�����Ⴄ */
            {
                ime_str_length = 0;
            }
            wParam = ((wParam >> 8) & 0xff) + ((wParam & 0xff) << 8);
            MultiByteToWideChar( CP_ACP, 0, (char *)&wParam, 2, (LPWSTR)&ime_buf[ime_str_length], 2 );
            ime_str_length += 1;
            ime_buf[ime_str_length] = 0;
            LeaveCriticalSection( &ime_cs );
            return 0;
        }
        break;
#endif

    case WM_APP + 0:
        /* �f�o�C�X�̃��Z�b�g */
        return reset();

    case WM_APP + 1:
        /* �E�B���h�E�̐ݒ�ύX */
        return ChangeSize();

    case WM_APP + 2:
        /* �J�[�\���̕\�� */
        while( ShowCursor( TRUE ) < 0 );
        g_WindowInfo.enablemouse = 1;
        return 0;

    case WM_APP + 3:
        /* �J�[�\���̔�\�� */
        while( ShowCursor( FALSE ) >= 0 );
        g_WindowInfo.enablemouse = 0;
        return 0;

    case WM_APP + 4:
        /* �J�[�\���̕ύX */
        SetCursor( LoadCursor( NULL, (LPSTR)lParam ));
        return 0;

#ifdef DXRUBY15
    case WM_APP + 6:
        /* IME�̗L����/������ */
        if( (int)lParam )
        {
            if( default_imc )
            {
                ImmAssociateContext( g_hWnd, default_imc );
                default_imc = NULL;
            }
        }
        else
        {
            if( !default_imc )
            {
                default_imc = ImmAssociateContext( g_hWnd, NULL );
            }
        }

        return 0;

    case WM_APP + 7:
        /* IME�̏�Ԃ�Ԃ� */
//        return WINNLSGetEnableStatus( g_hWnd );
        return !!default_imc;

    case WM_IME_STARTCOMPOSITION:
        ime_compositing = 1;
        return 0;

    case WM_IME_ENDCOMPOSITION:
        ime_compositing = 0;
        EnterCriticalSection( &ime_cs );
        if( ime_composition_str && ime_composition_str != ime_composition_str_old )
        {
            free( ime_composition_str );
        }
        ime_composition_str = NULL;

        if( ime_composition_attr && ime_composition_attr != ime_composition_attr_old )
        {
            free( ime_composition_attr );
        }
        ime_composition_attr = NULL;
        ime_composition_attr_size = 0;

        if( ime_canlist && ime_canlist != ime_canlist_old )
        {
            free( ime_canlist );
        }
        ime_canlist = NULL;

        ime_cursor_pos = 0;

        LeaveCriticalSection( &ime_cs );
        return 0;

    case WM_IME_COMPOSITION:
        {
            int len;
            HIMC hIMC = ImmGetContext( g_hWnd );
            int flag = 0;

            EnterCriticalSection( &ime_cs );
            if( lParam & GCS_COMPSTR )
            {
                if( ime_composition_str && ime_composition_str != ime_composition_str_old )
                {
                    free( ime_composition_str );
                }

                len = ImmGetCompositionStringW( hIMC, GCS_COMPSTR, 0, 0 );
                if( len )
                {
                    ime_composition_str = (WCHAR *)malloc( len + 2 );
                    len = ImmGetCompositionStringW( hIMC, GCS_COMPSTR, ime_composition_str, len );
                    ime_composition_str[len / 2] = 0;
                }
                else
                {
                    ime_composition_str = NULL;
                }
            }
            if( lParam & GCS_COMPATTR )
            {
                if( ime_composition_attr && ime_composition_attr != ime_composition_attr_old )
                {
                    free( ime_composition_attr );
                }

                len = ImmGetCompositionStringW( hIMC, GCS_COMPATTR, 0, 0 );
                if( len )
                {
                    ime_composition_attr = (char *)malloc( len );
                    ime_composition_attr_size = ImmGetCompositionStringW( hIMC, GCS_COMPATTR, ime_composition_attr, len );
                }
                else
                {
                    ime_composition_attr = NULL;
                    ime_composition_attr_size = 0;
                }
            }
            if( lParam & GCS_CURSORPOS )
            {
                ime_cursor_pos = ImmGetCompositionStringW( hIMC, GCS_CURSORPOS, NULL, 0 );
            }
            if( lParam & GCS_RESULTSTR )
            {
                WCHAR *temp;

                len = ImmGetCompositionStringW( hIMC, GCS_RESULTSTR, 0, 0 );

                if( len > IME_BUF_SIZE + 2 )
                {
                    return 0;
                }

                if( ime_str_length + len + 1 >= IME_BUF_SIZE-1 ) /* �o�b�t�@�I�[�o�[������N���A�����Ⴄ */
                {
                    ime_str_length = 0;
                }

                len = ImmGetCompositionStringW( hIMC, GCS_RESULTSTR, (LPWSTR)&ime_buf[ime_str_length], len );
                ime_str_length += len / 2;
                ime_buf[ime_str_length] = 0;
                flag = 1;
            }
            LeaveCriticalSection( &ime_cs );
            ImmReleaseContext( g_hWnd, hIMC );
            if( flag )
            {
                return 0;
            }
        }
        break;
    case WM_IME_SETCONTEXT:
        lParam &= ~ISC_SHOWUIALL;
        break;
    case WM_IME_NOTIFY:
        switch( wParam ) {
        case IMN_OPENSTATUSWINDOW:
        case IMN_CLOSESTATUSWINDOW:
            return 0;
        case IMN_OPENCANDIDATE:
        case IMN_CHANGECANDIDATE:
            {
                int len;
                HIMC hIMC;

                EnterCriticalSection( &ime_cs );
                hIMC = ImmGetContext( g_hWnd );

                if( ime_canlist && ime_canlist != ime_canlist_old )
                {
                    free( ime_canlist );
                }

                len = ImmGetCandidateListW( hIMC, 0, NULL, 0 );
                if( len )
                {
                  ime_canlist = (LPCANDIDATELIST)malloc( len );
                  ImmGetCandidateListW(hIMC, 0, ime_canlist, len);
                }
                else
                {
                    ime_canlist = NULL;
                }

                ImmReleaseContext( g_hWnd, hIMC );
                LeaveCriticalSection( &ime_cs );
                return 0;
            }
        case IMN_CLOSECANDIDATE:
            {
                EnterCriticalSection( &ime_cs );
                if( ime_canlist && ime_canlist != ime_canlist_old )
                {
                    free( ime_canlist );
                }
                ime_canlist = NULL;
                LeaveCriticalSection( &ime_cs );
                return 0;
            }
        default:
            break;
        }
#endif
    }

    /* �f�t�H���g���� */
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

/*--------------------------------------------------------------------
  �i�����֐��j�E�B���h�E�̐���
 ---------------------------------------------------------------------*/
static int InitWindow( void )
{
    WNDCLASSEX wcex;
    RECT rect;

    /* �E�C���h�E�E�N���X�̓o�^ */
    wcex.cbSize        = sizeof( WNDCLASSEX );
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = (WNDPROC)MessageThreadWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = g_hInstance;
    wcex.hIcon         = NULL;
    wcex.hIconSm       = NULL;
    wcex.hCursor       = NULL;//LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = "DXRuby";

    if( !RegisterClassEx( &wcex ) )
    {
        MainThreadError = 1;
        return MainThreadError;
    }

    /* ���C���E�E�C���h�E�쐬(�E�C���h�E�E���[�h�p) */
    rect.top     = 0;
    rect.left    = 0;
    rect.right   = g_WindowInfo.width;
    rect.bottom  = g_WindowInfo.height;

    AdjustWindowRect( &rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE );

    rect.right   = rect.right - rect.left;
    rect.bottom  = rect.bottom - rect.top;
    rect.left    = 0;
    rect.top     = 0;

    g_hWnd = CreateWindow( TEXT("DXRuby"), TEXT("DXRuby Application"),
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rect.right - rect.left, rect.bottom - rect.top,
                           NULL, NULL, g_hInstance, NULL );
    if( g_hWnd == NULL )
    {
        MainThreadError = 2;
        return MainThreadError;
    }

    GetWindowRect( g_hWnd, &rect );
    g_WindowInfo.x        = rect.left;
    g_WindowInfo.y        = rect.top;

    return 0;
}


/*--------------------------------------------------------------------
  �i�����֐��jDirectX Graphics������
 ---------------------------------------------------------------------*/
static int InitDXGraphics( void )
{
    D3DVIEWPORT9 vp;
    HRESULT hr;

    /* Direct3D�I�u�W�F�N�g�̍쐬 */
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

    if( g_pD3D == NULL )
    {
        MainThreadError = 3;
        return MainThreadError;
    }

    /* D3DDevice�I�u�W�F�N�g�̍쐬(�E�C���h�E�E���[�h) */
    ZeroMemory( &g_D3DPP, sizeof( g_D3DPP ) );

    g_D3DPP.BackBufferWidth            = 0;
    g_D3DPP.BackBufferHeight           = 0;
    g_D3DPP.BackBufferFormat           = D3DFMT_UNKNOWN;
    g_D3DPP.BackBufferCount            = 2;
    g_D3DPP.MultiSampleType            = D3DMULTISAMPLE_NONE;
    g_D3DPP.MultiSampleQuality         = 0;
    g_D3DPP.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    g_D3DPP.hDeviceWindow              = g_hWnd;
    g_D3DPP.Windowed                   = TRUE;
    g_D3DPP.EnableAutoDepthStencil     = TRUE;
    g_D3DPP.AutoDepthStencilFormat     = D3DFMT_D24S8;
    g_D3DPP.Flags                      = 0;
    g_D3DPP.FullScreen_RefreshRateInHz = 0;
    g_D3DPP.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;

    hr = g_pD3D->lpVtbl->CreateDevice( g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd,
                                       D3DCREATE_MIXED_VERTEXPROCESSING, &g_D3DPP, &g_pD3DDevice );

    if( FAILED( hr ) )
    {
        hr = g_pD3D->lpVtbl->CreateDevice( g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd,
                                           D3DCREATE_SOFTWARE_VERTEXPROCESSING, &g_D3DPP, &g_pD3DDevice );

        if( FAILED( hr ) )
        {
            MainThreadError = 4;
            return MainThreadError;
        }
    }

    /* �r���[�|�[�g�̐ݒ� */
    vp.X       = 0;
    vp.Y       = 0;
    vp.Width   = g_D3DPP.BackBufferWidth;
    vp.Height  = g_D3DPP.BackBufferHeight;
    vp.MinZ    = 0.0f;
    vp.MaxZ    = 1.0f;

    hr = g_pD3DDevice->lpVtbl->SetViewport( g_pD3DDevice, &vp );

    if( FAILED( hr ) )
    {
        MainThreadError = 5;
        return MainThreadError;
    }

    g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                 D3DCOLOR_XRGB(0,0,0), 1.0f, 0 );

    /* D3DXSprite�I�u�W�F�N�g�쐬 */
    hr = D3DXCreateSprite( g_pD3DDevice, &g_pD3DXSprite );

    if( FAILED( hr ) )
    {
        MainThreadError = 6;
        return MainThreadError;
    }

    return 0;
}

/*--------------------------------------------------------------------
   DirectX�I�u�W�F�N�g���������
 ---------------------------------------------------------------------*/
static void DXRelease()
{
    int i;
    HRESULT hr;

    /* �~�`��pShader��� */
    RELEASE( g_WindowInfo.pD3DXEffectCircleShader );
    RELEASE( g_WindowInfo.pD3DXEffectCircleFillShader );

    /* D3DXSprite�I�u�W�F�N�g�̎g�p�I�� */
    if( g_pD3DXSprite )
    {
        g_pD3DXSprite->lpVtbl->OnLostDevice( g_pD3DXSprite );
    }

    /* DirectInput��� */
    Input_release();

    /* D3DXSprite�I�u�W�F�N�g�j�� */
    RELEASE( g_pD3DXSprite );

    /* Direct3D Device�I�u�W�F�N�g�̔j�� */
    RELEASE( g_pD3DDevice );

	/* Direct3D�I�u�W�F�N�g�̔j�� */
    RELEASE( g_pD3D );
}


void InitMessageThread( void )
{
    hEventMainThreadStart = CreateEvent( NULL, FALSE, FALSE, NULL );

#ifdef DXRUBY15
    ime_str_length = 0;
    ime_buf = malloc( IME_BUF_SIZE * sizeof(WCHAR) );
    ime_buf[0] = 0;
    ime_buf_old = malloc( IME_BUF_SIZE * sizeof(WCHAR) );
    ime_buf_old[0] = 0;
    InitializeCriticalSection( &ime_cs );
    ime_compositing = 0;

    ime_vk_push_str_length = 0;
    ime_vk_push_buf = malloc( IME_VK_BUF_SIZE );
    ime_vk_push_buf[0] = 0;
    ime_vk_push_buf_old = malloc( IME_VK_BUF_SIZE );
    ime_vk_push_buf_old[0] = 0;

    ime_vk_release_str_length = 0;
    ime_vk_release_buf = malloc( IME_VK_BUF_SIZE );
    ime_vk_release_buf[0] = 0;
    ime_vk_release_buf_old = malloc( IME_VK_BUF_SIZE );
    ime_vk_release_buf_old[0] = 0;
#endif

    hMessageThread = CreateThread( NULL, 0, &MessageThreadProc, 0, 0, &MessageThreadID );
    {
        unsigned long result = WaitForSingleObject( hEventMainThreadStart, INFINITE );
        switch( MainThreadError )
        {
        case 0:
            break;
        case 1:
            DXRuby_raise( ERR_WINDOWCREATE, "RegisterClassEx" );
            break;
        case 2:
            DXRuby_raise( ERR_WINDOWCREATE, "CreateWindow" );
            break;
        case 3:
            DXRuby_raise( ERR_D3DERROR, "Direct3DCreate9" );
            break;
        case 4:
            DXRuby_raise( ERR_D3DERROR, "CreateDevice" );
            break;
        case 5:
            DXRuby_raise( ERR_D3DERROR, "SetViewport" );
            break;
        case 6:
            DXRuby_raise( ERR_D3DERROR, "D3DXCreateSprite" );
            break;
        case 7:
            DXRuby_raise( ERR_D3DERROR, "GetSwapChain" );
            break;
        case 10:
            DXRuby_raise( ERR_DINPUTERROR, "DirectInput8Create" );
            break;
        case 11:
            DXRuby_raise( ERR_DINPUTERROR, "CreateDevice" );
            break;
        case 12:
            DXRuby_raise( ERR_DINPUTERROR, "SetDataFormat" );
            break;
        case 13:
            DXRuby_raise( ERR_DINPUTERROR, "SetCooperativeLevel" );
            break;
        case 14:
            DXRuby_raise( ERR_DINPUTERROR, "EnumDevices" );
            break;
        case 15:
            DXRuby_raise( ERR_DINPUTERROR, "SetDataFormat" );
            break;
        case 16:
            DXRuby_raise( ERR_DINPUTERROR, "SetCooperativeLevel" );
            break;
        case 17:
            DXRuby_raise( ERR_DINPUTERROR, "GetProperty" );
            break;
        case 18:
            DXRuby_raise( ERR_DINPUTERROR, "EnumObjects" );
            break;
        }
    }
}

void ExitMessageThread( void )
{
    DWORD exitcode;

    if( g_hWnd != NULL )
    {
        SendMessage( g_hWnd, WM_DESTROY, 0, 0 );
    }

//    do
//    {
//        if( !GetExitCodeThread( hMessageThread, &exitcode ) )
//        {
//            rb_raise( eDXRubyError, "Therad exit error - ExitMessageThread" );
//        }
//        Sleep(1);
//    }
//    while( exitcode == STILL_ACTIVE );

#ifdef DXRUBY15
    free( ime_buf );
    free( ime_buf_old );
    free( ime_vk_push_buf );
    free( ime_vk_push_buf_old );
    free( ime_vk_release_buf );
    free( ime_vk_release_buf_old );
    if( ime_canlist )
    {
        free( ime_canlist );
    }
    if( ime_canlist_old && ime_canlist != ime_canlist_old )
    {
        free( ime_canlist_old );
    }
    if( ime_composition_str )
    {
        free( ime_composition_str );
    }
    if( ime_composition_str_old && ime_composition_str != ime_composition_str_old )
    {
        free( ime_composition_str_old );
    }
    if( ime_composition_attr )
    {
        free( ime_composition_attr );
    }
    if( ime_composition_attr_old && ime_composition_attr != ime_composition_attr_old )
    {
        free( ime_composition_attr_old );
    }

    DeleteCriticalSection( &ime_cs );
#endif

    /* �E�C���h�E�E�N���X�̓o�^���� */
    UnregisterClass( "DXRuby", g_hInstance );

    CloseHandle( hMessageThread );
}


void WindowCreateMessage( void )
{
    int ret;

    ret = SendMessage( g_hWnd, WM_APP + 1, 0, 0 );

    switch( ret )
    {
    case 0:
        break;
    case 1:
        DXRuby_raise( ERR_NOEXISTSCREENMODE, "Reset" );
        break;
    case 2:
        DXRuby_raise( ERR_INTERNAL, "D3DXCreateTexture" );
        break;
    case 3:
        DXRuby_raise( ERR_INTERNAL, "GetSurfaceLevel" );
        break;
    case 4:
        DXRuby_raise( ERR_INTERNAL, "TestCooperativeLevel" );
        break;
    }
}

int ResetMessage( void )
{
    int ret;
    /* �f�o�C�X���X�g�̏ꍇ�̏��� */
    ret = SendMessage( g_hWnd, WM_APP + 0, 0, 0 );
    switch( ret )
    {
    case 0:
    case 5:
        break;
    case 1:
        DXRuby_raise( ERR_INTERNAL, "Reset" );
        break;
    case 2:
        DXRuby_raise( ERR_INTERNAL, "D3DXCreateTexture" );
        break;
    case 3:
        DXRuby_raise( ERR_INTERNAL, "GetSurfaceLevel" );
        break;
    case 4:
        DXRuby_raise( ERR_INTERNAL, "TestCooperativeLevel" );
        break;
    case 10:
        DXRuby_raise( ERR_INTERNAL, "Reset_InvaridCall" );
        break;
    }
    Sleep(100); /* �Ƃ肠����wait */
    return ret;
}

void ShowCursorMessage( void )
{
    SendMessage( g_hWnd, WM_APP + 2, 0, 0 );
}

void HideCursorMessage( void )
{
    SendMessage( g_hWnd, WM_APP + 3, 0, 0 );
}

void SetCursorMessage( int cursorname )
{
    SendMessage( g_hWnd, WM_APP + 4, 0, cursorname );
}

void SetImeEnable( int flag )
{
    SendMessage( g_hWnd, WM_APP + 6, 0, flag );
}

int GetImeEnable( void )
{
    return SendMessage( g_hWnd, WM_APP + 7, 0, 0 );
}
