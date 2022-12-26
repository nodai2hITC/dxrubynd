/* �A�i���O�p�b�h���͂̃R�[�h�� @iMAKOPi�����蒸���܂����B2012/07/06 */
#define WINVER 0x0500                                  /* �o�[�W������` Windows2000�ȏ� */
#define _WIN32_WINNT WINVER
#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION        (0x0800)            /* DirectInput�o�[�W������` */

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif
#include <dinput.h>

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "input.h"
#ifdef DXRUBY15
#include "font.h"
#endif

#ifdef DXRUBY15
#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Font_data_type;
#endif
#endif

#ifndef DIK_PREVTRACK
#define DIK_PREVTRACK DIK_CIRCUMFLEX
#endif

#define PADMAX 2

/* �Ǘ�����{�^���̌� */
#define PADBUTTON_MAX 28

#define P_LEFT     0
#define P_RIGHT    1
#define P_UP       2
#define P_DOWN     3
#define P_BUTTON0  4
#define P_BUTTON1  5
#define P_BUTTON2  6
#define P_BUTTON3  7
#define P_BUTTON4  8
#define P_BUTTON5  9
#define P_BUTTON6  10
#define P_BUTTON7  11
#define P_BUTTON8  12
#define P_BUTTON9  13
#define P_BUTTON10 14
#define P_BUTTON11 15
#define P_BUTTON12 16
#define P_BUTTON13 17
#define P_BUTTON14 18
#define P_BUTTON15 19

/* ���A�i���O�X�e�B�b�N */
#define P_L_LEFT     0
#define P_L_RIGHT    1
#define P_L_UP       2
#define P_L_DOWN     3

/* �f�W�^���\���{�^�� */
#define P_D_LEFT     20
#define P_D_RIGHT    21
#define P_D_UP       22
#define P_D_DOWN     23

/* �E�A�i���O�X�e�B�b�N */
#define P_R_LEFT     24
#define P_R_RIGHT    25
#define P_R_UP       26
#define P_R_DOWN     27

#define M_LBUTTON   0
#define M_RBUTTON   1
#define M_MBUTTON   2

/* POV�̕�������p */
#define POV_UP          0
#define POV_UP_RIGHT    4500
#define POV_RIGHT       9000
#define POV_DOWN_RIGHT  13500
#define POV_DOWN        18000
#define POV_DOWN_LEFT   22500
#define POV_LEFT        27000
#define POV_UP_LEFT     31500


static VALUE mInput;        /* �C���v�b�g���W���[�� */
static VALUE mIME;          /* IME���W���[�� */
static LPDIRECTINPUT8        g_pDInput            = NULL; /* DirectInput */
static LPDIRECTINPUTDEVICE8  g_pDIDKeyBoard       = NULL; /* DirectInput�̃L�[�{�[�h�f�o�C�X */
static LPDIRECTINPUTDEVICE8  g_pDIDJoyPad[PADMAX];        /* DirectInput�̃p�b�h�f�o�C�X     */
static BYTE g_diKeyState[256];                            /* DirectInput�ł̃L�[�{�[�h���͗p�o�b�t�@ */
static BYTE g_diKeyStateOld[256];                         /* DirectInput�ł̃L�[�{�[�h���͗p�o�b�t�@�P�t���[���O */
static BYTE g_diKeyCount[256];                            /* DirectInput�ł̃L�[�{�[�h���͗p�J�E���^ */
static BYTE g_diKeyConfig[256];                           /* DirectInput�ł̃L�[�{�[�h�E�p�b�h���蓖�� */
static BYTE g_diKeyWait[256];                             /* �I�[�g���s�[�g�E�F�C�g���� */
static BYTE g_diKeyInterval[256];                         /* �I�[�g���s�[�g�Ԋu */
static int g_JoystickCount = 0;
static BYTE g_byMouseState_L, g_byMouseStateOld_L;
static BYTE g_byMouseState_M, g_byMouseStateOld_M;
static BYTE g_byMouseState_R, g_byMouseStateOld_R;

#ifdef DXRUBY15
extern WCHAR *ime_buf;
extern int ime_str_length;
extern int ime_str_length_old;
extern WCHAR *ime_buf_old;
extern CRITICAL_SECTION ime_cs;
extern int ime_compositing;
extern char *ime_vk_push_buf;
extern int ime_vk_push_str_length;
extern char *ime_vk_push_buf_old;
int ime_vk_push_str_length_old;
extern char *ime_vk_release_buf;
extern int ime_vk_release_str_length;
extern char *ime_vk_release_buf_old;
int ime_vk_release_str_length_old;
extern WCHAR *ime_composition_str;
extern WCHAR *ime_composition_str_old;
extern char *ime_composition_attr;
extern char *ime_composition_attr_old;
extern int ime_composition_attr_size;
extern LPCANDIDATELIST ime_canlist;
extern LPCANDIDATELIST ime_canlist_old;
extern int ime_cursor_pos;

static VALUE g_vCompInfoArray = Qnil;;

static VALUE cCompInfo;     /* �ϊ����N���X   */
#endif

//LPDIRECTINPUTEFFECT  g_lpDIEffect = NULL;

/* Pad��� */
static struct DXRubyPadInfo {
    /* �e���͈̔� */
    int x_flg;
    int x_min;
    int x_center;
    int x_max;
    int y_flg;
    int y_min;
    int y_center;
    int y_max;
    int z_flg;
    int z_min;
    int z_center;
    int z_max;
    int rx_flg;
    int rx_min;
    int rx_center;
    int rx_max;
    int ry_flg;
    int ry_min;
    int ry_center;
    int ry_max;
    int rz_flg;
    int rz_min;
    int rz_center;
    int rz_max;

    int x_ff_flg;
    int y_ff_flg;

    LPDIRECTINPUTDEVICE8  pDIDJoyPad; /* DirectInput�̃p�b�h�f�o�C�X */
} g_PadInfo[PADMAX];

/* Pad��� */
static struct DXRubyPadState {
    char button[PADBUTTON_MAX];
    int PadConfig[PADBUTTON_MAX];
    int count[PADBUTTON_MAX];
    int wait[PADBUTTON_MAX];
    int interval[PADBUTTON_MAX];
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float rz;
    int pov;
} g_PadState[PADMAX], g_PadStateOld[PADMAX];

int Window_autocall_foreach( VALUE key, VALUE value, VALUE obj );

/*********************************************************************
 * Input���W���[��
 *
 * DirectInput���g�p���ăL�[�{�[�h�E�p�b�h�̓��͂��s���B
 *********************************************************************/

/*--------------------------------------------------------------------
   Windows���b�Z�[�W�������݂̓��͍X�V
 ---------------------------------------------------------------------*/
VALUE Input_update( VALUE obj )
{
//    if( g_WindowInfo.input_updated == 0 )
//    {
        /* ���͏�ԍX�V */
        inputupdate_internal();
        rb_hash_foreach( g_WindowInfo.before_call, Window_autocall_foreach, obj );
//    }

    if( g_WindowInfo.requestclose == 1 )
    {
        g_WindowInfo.requestclose = 0;
        return Qtrue;
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   �����悤�Ƃ������ǂ�����Ԃ�
 ---------------------------------------------------------------------*/
VALUE Input_requested_close( VALUE obj )
{
    if( g_WindowInfo.requestclose == 1 )
    {
        g_WindowInfo.requestclose = 0;
        return Qtrue;
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̓��͏���
 ---------------------------------------------------------------------*/
void inputupdate_internal( void )
{
    int i, j;
    DIJOYSTATE paddata;
    WCHAR *ime_buf_tmp_wchar;
    char *ime_buf_tmp_char;

    /* �t�H�[�J�X���͂��ꂽ�ꍇ�A�L�[�ƃp�b�h�A�}�E�X�{�^���̓��͂��󂯕t���Ȃ����� */
    if( g_WindowInfo.active == 0 )
    {
        /* �L�[�{�[�h�̃f�[�^���N���A���� */
        ZeroMemory( &g_diKeyState, sizeof(g_diKeyState) );
        ZeroMemory( &g_diKeyStateOld, sizeof(g_diKeyStateOld) );

        /* �p�b�h�f�[�^���N���A���� */
        for( i = 0; i < g_JoystickCount; i++ )
        {
            for( j = 0; j < PADBUTTON_MAX; j++ )
            {
                g_PadStateOld[i].button[j] = g_PadState[i].button[j];
                g_PadState[i].button[j] = 0;
            }
        }

        /* �}�E�X�{�^���̃f�[�^���N���A���� */
        g_byMouseStateOld_L = g_byMouseState_L;
        g_byMouseStateOld_M = g_byMouseState_M;
        g_byMouseStateOld_R = g_byMouseState_R;
        g_byMouseState_L = 0;
        g_byMouseState_M = 0;
        g_byMouseState_R = 0;

        g_WindowInfo.input_updated = 1;
        return;
    }

    /* �f�o�C�X�A�N�Z�X�����Ď擾���� */
    g_pDIDKeyBoard->lpVtbl->Acquire( g_pDIDKeyBoard );

    for( i = 0; i < g_JoystickCount; i++ )
    {
        g_pDIDJoyPad[i]->lpVtbl->Poll( g_pDIDJoyPad[i] );
        g_pDIDJoyPad[i]->lpVtbl->Acquire( g_pDIDJoyPad[i] );
    }

    /* �L�[�{�[�h�̒��ڃf�[�^���擾���� */
    memcpy( g_diKeyStateOld, g_diKeyState, sizeof(g_diKeyState) );
    g_pDIDKeyBoard->lpVtbl->GetDeviceState( g_pDIDKeyBoard, 256, g_diKeyState );

    for( i = 0; i < PADMAX; i++ )
    {
        for( j = 0; j < PADBUTTON_MAX; j++ )
        {
            g_PadStateOld[i].button[j] = g_PadState[i].button[j];
            g_PadState[i].button[j] = 0;
            g_PadState[i].count[j]++; /* �J�E���g */
        }
    }

    /* �Q�[���p�b�h�̃f�[�^���擾���� */
    for( i = 0; i < g_JoystickCount; i++ )
    {
        int j;

        g_pDIDJoyPad[i]->lpVtbl->GetDeviceState( g_pDIDJoyPad[i], sizeof(DIJOYSTATE), &paddata );

//        //----------------------------------------
//        // typedef struct DIJOYSTATE {
//        //     LONG lX;
//        //     LONG lY;
//        //     LONG lZ;
//        //     LONG lRx;
//        //     LONG lRy;
//        //     LONG lRz;
//        //     LONG rglSlider[2];
//        //     DWORD rgdwPOV[4];
//        //     BYTE rgbButtons[32];
//        // } DIJOYSTATE, *LPDIJOYSTATE;
//        //----------------------------------------
//        {
//          int tmp = 0;
//          
//          tmp  =  paddata.rgbButtons[ 0] >> 7;
//          tmp |= (paddata.rgbButtons[ 1] >> 7) <<  1;
//          tmp |= (paddata.rgbButtons[ 2] >> 7) <<  2;
//          tmp |= (paddata.rgbButtons[ 3] >> 7) <<  3;
//          tmp |= (paddata.rgbButtons[ 4] >> 7) <<  4;
//          tmp |= (paddata.rgbButtons[ 5] >> 7) <<  5;
//          tmp |= (paddata.rgbButtons[ 6] >> 7) <<  6;
//          tmp |= (paddata.rgbButtons[ 7] >> 7) <<  7;
//          tmp |= (paddata.rgbButtons[ 8] >> 7) <<  8;
//          tmp |= (paddata.rgbButtons[ 9] >> 7) <<  9;
//          tmp |= (paddata.rgbButtons[10] >> 7) << 10;
//          tmp |= (paddata.rgbButtons[11] >> 7) << 11;
//          tmp |= (paddata.rgbButtons[12] >> 7) << 12;
//          tmp |= (paddata.rgbButtons[13] >> 7) << 13;
//          tmp |= (paddata.rgbButtons[14] >> 7) << 14;
//          tmp |= (paddata.rgbButtons[15] >> 7) << 15;
//          tmp |= (paddata.rgbButtons[16] >> 7) << 16;
//          tmp |= (paddata.rgbButtons[17] >> 7) << 17;
//          tmp |= (paddata.rgbButtons[18] >> 7) << 18;
//          tmp |= (paddata.rgbButtons[19] >> 7) << 19;
//          tmp |= (paddata.rgbButtons[20] >> 7) << 20;
//          tmp |= (paddata.rgbButtons[21] >> 7) << 21;
//          tmp |= (paddata.rgbButtons[22] >> 7) << 22;
//          tmp |= (paddata.rgbButtons[23] >> 7) << 23;
//          tmp |= (paddata.rgbButtons[24] >> 7) << 24;
//          tmp |= (paddata.rgbButtons[25] >> 7) << 25;
//          tmp |= (paddata.rgbButtons[26] >> 7) << 26;
//          tmp |= (paddata.rgbButtons[27] >> 7) << 27;
//          tmp |= (paddata.rgbButtons[28] >> 7) << 28;
//          tmp |= (paddata.rgbButtons[29] >> 7) << 29;
//          tmp |= (paddata.rgbButtons[30] >> 7) << 30;
//          tmp |= (paddata.rgbButtons[31] >> 7) << 31;
//
//
//          printf("button=%08X lX=%08X lY=%08X lZ=%08X lRx=%08X lRy=%08X lRz=%08X rglSlider[0]=%08X rgdwPOV[0]=%d\n",
//                 tmp,
//                 paddata.lX, paddata.lY, paddata.lZ,
//                 paddata.lRx, paddata.lRy, paddata.lRz,
//                 paddata.rglSlider[0], paddata.rgdwPOV[0] );
//        
//          //debug
//          printf("g_PadInfo[%d].right = 0x%08X\n", i, g_PadInfo[i].right);
//          printf("g_PadInfo[%d].left  = 0x%08X\n", i, g_PadInfo[i].left);
//          printf("g_PadInfo[%d].up    = 0x%08X\n", i, g_PadInfo[i].up);
//          printf("g_PadInfo[%d].down  = 0x%08X\n", i, g_PadInfo[i].down);
//          printf("g_PadInfo[%d].forward = 0x%08X\n", i, g_PadInfo[i].forward);
//          printf("g_PadInfo[%d].backward = 0x%08X\n", i, g_PadInfo[i].backward);
//
//          printf("g_PadInfo[%d].r_right = 0x%08X\n", i, g_PadInfo[i].r_right);
//          printf("g_PadInfo[%d].r_left  = 0x%08X\n", i, g_PadInfo[i].r_left);
//          printf("g_PadInfo[%d].r_up    = 0x%08X\n", i, g_PadInfo[i].r_up);
//          printf("g_PadInfo[%d].r_down  = 0x%08X\n", i, g_PadInfo[i].r_down);
//          printf("g_PadInfo[%d].r_forward = 0x%08X\n", i, g_PadInfo[i].r_forward);
//          printf("g_PadInfo[%d].r_backward = 0x%08X\n", i, g_PadInfo[i].r_backward);
//        }

        /* �e����0.0�`1.0�̒l�Ɍv�Z */
        g_PadState[i].x = g_PadInfo[i].x_flg ? (float)(paddata.lX - g_PadInfo[i].x_min) / (g_PadInfo[i].x_max - g_PadInfo[i].x_min) : 0;
        g_PadState[i].y = g_PadInfo[i].y_flg ? (float)(paddata.lY - g_PadInfo[i].y_min) / (g_PadInfo[i].y_max - g_PadInfo[i].y_min) : 0;
        g_PadState[i].z = g_PadInfo[i].z_flg ? (float)(paddata.lZ - g_PadInfo[i].z_min) / (g_PadInfo[i].z_max - g_PadInfo[i].z_min) : 0;
        g_PadState[i].rx = g_PadInfo[i].rx_flg ? (float)(paddata.lRx - g_PadInfo[i].rx_min) / (g_PadInfo[i].rx_max - g_PadInfo[i].rx_min) : 0;
        g_PadState[i].ry = g_PadInfo[i].ry_flg ? (float)(paddata.lRy - g_PadInfo[i].ry_min) / (g_PadInfo[i].ry_max - g_PadInfo[i].ry_min) : 0;
        g_PadState[i].rz = g_PadInfo[i].rz_flg ? (float)(paddata.lRz - g_PadInfo[i].rz_min) / (g_PadInfo[i].rz_max - g_PadInfo[i].rz_min) : 0;

        /* POV�̒l�ۑ� */
        g_PadState[i].pov = paddata.rgdwPOV[0];

        /* �p�b�h�̏\���{�^��(�f�W�^���̏ꍇ)or���X�e�B�b�N(�A�i���O�̏ꍇ) */
        /* �� */
        if( paddata.lX < (g_PadInfo[i].x_center + g_PadInfo[i].x_min) / 2 )
        {
            g_PadState[i].button[P_LEFT] = 1;
        }
        /* �E */
        else if( paddata.lX > (g_PadInfo[i].x_center + g_PadInfo[i].x_max ) / 2 )
        {
            g_PadState[i].button[P_RIGHT] = 1;
        }

        /* �� */
        if( paddata.lY < (g_PadInfo[i].y_center + g_PadInfo[i].y_min) / 2 )
        {
            g_PadState[i].button[P_UP] = 1;
        }
        /* �� */
        else if( paddata.lY > (g_PadInfo[i].y_center + g_PadInfo[i].y_max) / 2 )
        {
            g_PadState[i].button[P_DOWN] = 1;
        }

        /* �{�^����� */
        for( j = 0; j < 16; j++)
        {
            g_PadState[i].button[j + 4] = paddata.rgbButtons[j] >> 7;
        }

        /* �E�A�i���O:�� */
        if( paddata.lZ < (g_PadInfo[i].z_center + g_PadInfo[i].z_min) / 2 )
        {
            g_PadState[i].button[P_R_LEFT] = 1;
        }
        /* �E�A�i���O:�E */
        else if( paddata.lZ > (g_PadInfo[i].z_center + g_PadInfo[i].z_max ) / 2 )
        {
            g_PadState[i].button[P_R_RIGHT] = 1;
        }

        /* �E�A�i���O:�� */
        if( paddata.lRz < (g_PadInfo[i].rz_center + g_PadInfo[i].rz_min) / 2 )
        {
            g_PadState[i].button[P_R_UP] = 1;
        }
        /* �E�A�i���O:�� */
        else if( paddata.lRz > (g_PadInfo[i].rz_center + g_PadInfo[i].rz_max ) / 2 )
        {
            g_PadState[i].button[P_R_DOWN] = 1;
        }

        /* �\���{�^��(POV)�F�� */
        if( paddata.rgdwPOV[0] == POV_UP )
        {
            g_PadState[i].button[P_D_UP] = 1;
        }
        /* �\���{�^��(POV)�F�E�� */
        else if( paddata.rgdwPOV[0] == POV_UP_RIGHT )
        {
            g_PadState[i].button[P_D_UP] = 1;
            g_PadState[i].button[P_D_RIGHT] = 1;
        }
        /* �\���{�^��(POV)�F�E */
        else if( paddata.rgdwPOV[0] == POV_RIGHT )
        {
            g_PadState[i].button[P_D_RIGHT] = 1;
        }
        /* �\���{�^��(POV)�F�E�� */
        else if( paddata.rgdwPOV[0] == POV_DOWN_RIGHT )
        {
            g_PadState[i].button[P_D_DOWN] = 1;
            g_PadState[i].button[P_D_RIGHT] = 1;
        }
        /* �\���{�^��(POV)�F�� */
        else if( paddata.rgdwPOV[0] == POV_DOWN )
        {
            g_PadState[i].button[P_D_DOWN] = 1;
        }
        /* �\���{�^��(POV)�F���� */
        else if( paddata.rgdwPOV[0] == POV_DOWN_LEFT )
        {
            g_PadState[i].button[P_D_DOWN] = 1;
            g_PadState[i].button[P_D_LEFT] = 1;
        }
        /* �\���{�^��(POV)�F�� */
        else if( paddata.rgdwPOV[0] == POV_LEFT )
        {
            g_PadState[i].button[P_D_LEFT] = 1;
        }
        /* �\���{�^��(POV)�F���� */
        else if( paddata.rgdwPOV[0] == POV_UP_LEFT )
        {
            g_PadState[i].button[P_D_UP] = 1;
            g_PadState[i].button[P_D_LEFT] = 1;
        }
    }

    /* �}�E�X�{�^���̏�� */
    g_byMouseStateOld_L = g_byMouseState_L;
    g_byMouseStateOld_M = g_byMouseState_M;
    g_byMouseStateOld_R = g_byMouseState_R;
    g_byMouseState_L = g_byMouseState_L_buf;
    g_byMouseState_M = g_byMouseState_M_buf;
    g_byMouseState_R = g_byMouseState_R_buf;

    for( i = 0; i < 256; i++ )
    {
        g_diKeyCount[i]++; /* �J�E���g */
    }

#ifdef DXRUBY15
    /* �L�[�o�b�t�@�ۑ� */
    EnterCriticalSection( &ime_cs );
    ime_buf_tmp_wchar = ime_buf_old;
    ime_buf_old = ime_buf;
    ime_str_length_old = ime_str_length;
    ime_buf = ime_buf_tmp_wchar;
    ime_str_length = 0;
    ime_buf[0] = 0;

    ime_buf_tmp_char = ime_vk_push_buf_old;
    ime_vk_push_buf_old = ime_vk_push_buf;
    ime_vk_push_buf = ime_buf_tmp_char;
    ime_vk_push_str_length_old = ime_vk_push_str_length;
    ime_vk_push_str_length = 0;
    ime_vk_push_buf[0] = 0;

    ime_buf_tmp_char = ime_vk_release_buf_old;
    ime_vk_release_buf_old = ime_vk_release_buf;
    ime_vk_release_buf = ime_buf_tmp_char;
    ime_vk_release_str_length_old = ime_vk_release_str_length;
    ime_vk_release_str_length = 0;
    ime_vk_release_buf[0] = 0;

    {
        int i, j, topindex, pagesize;
        VALUE ary[8];

        if( ime_composition_str != ime_composition_str_old )
        {
            if( ime_composition_str_old )
            {
                free( ime_composition_str_old );
            }

            if( ime_composition_str )
            {
                ary[0] = rb_str_export_to_enc( rb_enc_associate( rb_str_new( (char*)ime_composition_str, wcslen( ime_composition_str ) * 2 ), g_enc_utf16 ), g_enc_utf8 );
            }
            else
            {
                ary[0] = rb_str_new2("");
            }
            ime_composition_str_old = ime_composition_str;
        }
        else
        {
            ary[0] = rb_ary_entry( g_vCompInfoArray, 0 );
        }

        if( ime_composition_attr != ime_composition_attr_old )
        {
            VALUE vattr_ary = rb_ary_new();
            if( ime_composition_attr_old )
            {
                free( ime_composition_attr_old );
            }

            if( ime_composition_attr )
            {
                for( i = 0; i < ime_composition_attr_size; i++ )
                {
                    rb_ary_push( vattr_ary, INT2FIX( ime_composition_attr[i] ) );
                }
            }
            ary[1] = vattr_ary;
            ime_composition_attr_old = ime_composition_attr;
        }
        else
        {
            ary[1] = rb_ary_entry( g_vCompInfoArray, 1 );
        }

        ary[2] = INT2NUM( ime_cursor_pos );

        if( ime_canlist != ime_canlist_old )
        {
            if( ime_canlist_old )
            {
                free( ime_canlist_old );
            }

            if( ime_canlist )
            {
                VALUE vcanlist = rb_ary_new();
                pagesize = ime_canlist->dwPageSize;
                topindex = (ime_canlist->dwSelection / pagesize) * pagesize;
                for( i = topindex, j = 0; i < ime_canlist->dwCount && j < pagesize; i++, j++ )
                {
                    rb_ary_push( vcanlist, rb_str_export_to_enc( rb_enc_associate( rb_str_new( (char *)ime_canlist + ime_canlist->dwOffset[i], wcslen( (const wchar_t *)((char *)ime_canlist + ime_canlist->dwOffset[i] )) * 2 ), g_enc_utf16 ), g_enc_utf8 ) );
                }
                ary[3] = vcanlist;
                ary[4] = INT2NUM( ime_canlist->dwSelection - topindex );
                ary[5] = INT2NUM( ime_canlist->dwSelection );
                ary[6] = INT2NUM( pagesize );
                ary[7] = INT2NUM( ime_canlist->dwCount );
            }
            else
            {
                ary[3] = rb_ary_new();
                ary[4] = Qnil;
                ary[5] = Qnil;
                ary[6] = Qnil;
                ary[7] = Qnil;
            }

            ime_canlist_old = ime_canlist;
        }
        else
        {
            ary[3] = rb_ary_entry( g_vCompInfoArray, 3 );
            ary[4] = rb_ary_entry( g_vCompInfoArray, 4 );
            ary[5] = rb_ary_entry( g_vCompInfoArray, 5 );
            ary[6] = rb_ary_entry( g_vCompInfoArray, 6 );
            ary[7] = rb_ary_entry( g_vCompInfoArray, 7 );
        }

        g_vCompInfoArray = rb_ary_new4( 8, ary );
    }
    LeaveCriticalSection( &ime_cs );
#endif

    g_WindowInfo.input_updated = 1;
    return;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �������̓��͂�x�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_x( int argc, VALUE *argv, VALUE obj )
{
    int x = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_x" );
    }

//    printf("PadConfig[P_LEFT] = %02X button[P_LEFT] = %d PadConfig[P_RIGHT] = %02X button[P_RIGHT] = %d\n", 
//           g_diKeyState[g_PadState[number].PadConfig[P_LEFT]],
//           g_PadState[number].button[P_LEFT],
//           g_diKeyState[g_PadState[number].PadConfig[P_RIGHT]],
//           g_PadState[number].button[P_RIGHT]
//           );


	/* �Ђ��� */
    if( (g_PadState[number].PadConfig[P_LEFT] != -1   && g_diKeyState[g_PadState[number].PadConfig[P_LEFT]] & 0x80)   || g_PadState[number].button[P_LEFT] == 1 ||
        (g_PadState[number].PadConfig[P_D_LEFT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_LEFT]] & 0x80) || g_PadState[number].button[P_D_LEFT] == 1 )
    {
        x = x - 1;
    }

    /* �݂� */
    if( (g_PadState[number].PadConfig[P_RIGHT] != -1   && g_diKeyState[g_PadState[number].PadConfig[P_RIGHT]] & 0x80)   || g_PadState[number].button[P_RIGHT] == 1 ||
        (g_PadState[number].PadConfig[P_D_RIGHT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_RIGHT]] & 0x80) || g_PadState[number].button[P_D_RIGHT] == 1 )
    {
        x = x + 1;
    }

    return INT2FIX( x );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �c�����̓��͂�y�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_y( int argc, VALUE *argv, VALUE obj )
{
    int y = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_y" );
    }


    /* ���� */
    if( (g_PadState[number].PadConfig[P_UP] != -1   && g_diKeyState[g_PadState[number].PadConfig[P_UP]] & 0x80)   || g_PadState[number].button[P_UP] == 1 ||
        (g_PadState[number].PadConfig[P_D_UP] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_UP]] & 0x80) || g_PadState[number].button[P_D_UP] == 1 )
    {
        y = y - 1;
    }

    /* ���� */
    if( (g_PadState[number].PadConfig[P_DOWN] != -1   && g_diKeyState[g_PadState[number].PadConfig[P_DOWN]] & 0x80)   || g_PadState[number].button[P_DOWN] == 1 ||
        (g_PadState[number].PadConfig[P_D_DOWN] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_DOWN]] & 0x80) || g_PadState[number].button[P_D_DOWN] == 1 )
    {
        y = y + 1;
    }

    return INT2FIX( y );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(���X�e�B�b�N)

   ���X�e�B�b�N�̉������̓��͂�x�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_lx( int argc, VALUE *argv, VALUE obj )
{
    int x = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_rx" );
    }

	/* �Ђ��� */
    if( (g_PadState[number].PadConfig[P_LEFT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_LEFT]] & 0x80) || g_PadState[number].button[P_LEFT] == 1 )
    {
        x = x - 1;
    }

    /* �݂� */
    if( (g_PadState[number].PadConfig[P_RIGHT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_RIGHT]] & 0x80) || g_PadState[number].button[P_RIGHT] == 1 )
    {
        x = x + 1;
    }

    return INT2FIX( x );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(���X�e�B�b�N)

   ���X�e�B�b�N�̏c�����̓��͂�y�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_ly( int argc, VALUE *argv, VALUE obj )
{
    int y = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_ry" );
    }


    /* ���� */
    if( (g_PadState[number].PadConfig[P_UP] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_UP]] & 0x80) || g_PadState[number].button[P_UP] == 1 )
    {
        y = y - 1;
    }

    /* ���� */
    if( (g_PadState[number].PadConfig[P_DOWN] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_DOWN]] & 0x80) || g_PadState[number].button[P_DOWN] == 1 )
    {
        y = y + 1;
    }

    return INT2FIX( y );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(�E�X�e�B�b�N)

   �E�X�e�B�b�N�̉������̓��͂�x�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_rx( int argc, VALUE *argv, VALUE obj )
{
    int x = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_rx" );
    }

	/* �Ђ��� */
    if( (g_PadState[number].PadConfig[P_R_LEFT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_R_LEFT]] & 0x80) || g_PadState[number].button[P_R_LEFT] == 1 )
    {
        x = x - 1;
    }

    /* �݂� */
    if( (g_PadState[number].PadConfig[P_R_RIGHT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_R_RIGHT]] & 0x80) || g_PadState[number].button[P_R_RIGHT] == 1 )
    {
        x = x + 1;
    }

    return INT2FIX( x );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(�E�X�e�B�b�N)

   �E�X�e�B�b�N�̏c�����̓��͂�y�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_ry( int argc, VALUE *argv, VALUE obj )
{
    int y = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_ry" );
    }


    /* ���� */
    if( (g_PadState[number].PadConfig[P_R_UP] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_R_UP]] & 0x80) || g_PadState[number].button[P_R_UP] == 1 )
    {
        y = y - 1;
    }

    /* ���� */
    if( (g_PadState[number].PadConfig[P_R_DOWN] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_R_DOWN]] & 0x80) || g_PadState[number].button[P_R_DOWN] == 1 )
    {
        y = y + 1;
    }

    return INT2FIX( y );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(�\���{�^��)

   �\���{�^���̉������̓��͂�x�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_dx( int argc, VALUE *argv, VALUE obj )
{
    int x = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_dx" );
    }


	/* �Ђ��� */
    if( (g_PadState[number].PadConfig[P_D_LEFT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_LEFT]] & 0x80) || g_PadState[number].button[P_D_LEFT] == 1 )
    {
        x = x - 1;
    }

    /* �݂� */
    if( (g_PadState[number].PadConfig[P_D_RIGHT] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_RIGHT]] & 0x80) || g_PadState[number].button[P_D_RIGHT] == 1 )
    {
        x = x + 1;
    }

    return INT2FIX( x );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾(�\���{�^��)

   �\���{�^���̏c�����̓��͂�y�̑����ŕԂ�
 ---------------------------------------------------------------------*/
static VALUE Input_dy( int argc, VALUE *argv, VALUE obj )
{
    int y = 0;
	VALUE vnumber;
	int number;

    rb_scan_args( argc, argv, "01", &vnumber);

	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_dy" );
    }


    /* ���� */
    if( (g_PadState[number].PadConfig[P_D_UP] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_UP]] & 0x80) || g_PadState[number].button[P_D_UP] == 1 )
    {
        y = y - 1;
    }

    /* ���� */
    if( (g_PadState[number].PadConfig[P_D_DOWN] != -1 && g_diKeyState[g_PadState[number].PadConfig[P_D_DOWN]] & 0x80) || g_PadState[number].button[P_D_DOWN] == 1 )
    {
        y = y + 1;
    }

    return INT2FIX( y );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   ������Ă�����true�ɂȂ�B�����̓L�[�R�[�h�B
 ---------------------------------------------------------------------*/
static VALUE Input_keyDown( VALUE obj , VALUE vkey )
{
    int key, padbutton = 0;

    key = NUM2INT( vkey );
    if( key < 0 || key >= 256 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_keyDown" );
    }

    //    if( g_diKeyConfig[key] != -1 && g_PadState[g_diKeyConfig[key] / 20].button[g_diKeyConfig[key] % 20] == 1 )
    if( g_diKeyConfig[key] != -1 && g_PadState[g_diKeyConfig[key] / PADBUTTON_MAX].button[g_diKeyConfig[key] % PADBUTTON_MAX] == 1 )
    {
        padbutton = 1;
    }
    
    if( g_diKeyState[key] & 0x80 ||  padbutton == 1)
    {
        return Qtrue;
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �������u�Ԃ���true�ɂȂ�B�����̓L�[�R�[�h�B
 ---------------------------------------------------------------------*/
static VALUE Input_keyPush( VALUE obj , VALUE vkey )
{
    int key, padbutton = 0, padbuttonold = 0;

    key = NUM2INT( vkey );
    if( key < 0 || key >= 256 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_keyPush" );
    }

    if( g_diKeyConfig[key] != -1 )
    {
        //if( g_PadState[g_diKeyConfig[key] / 20].button[g_diKeyConfig[key] % 20] == 1 )
        if( g_PadState[g_diKeyConfig[key] / PADBUTTON_MAX].button[g_diKeyConfig[key] % PADBUTTON_MAX] == 1 )
        {
            padbutton = 1;
        }
        //if( g_PadStateOld[g_diKeyConfig[key] / 20].button[g_diKeyConfig[key] % 20] == 1 )
        if( g_PadStateOld[g_diKeyConfig[key] / PADBUTTON_MAX].button[g_diKeyConfig[key] % PADBUTTON_MAX] == 1 )
        {
            padbuttonold = 1;
        }
    }

    if( (g_diKeyState[key] & 0x80) ||  padbutton == 1 ) /* ���͂���Ă��� */
    {
        if( !(g_diKeyStateOld[key] & 0x80) && padbuttonold == 0 )    /* �O��OFF�ł��� */
        {
            g_diKeyCount[key] = 0;                  /* �J�E���g������ */
            return Qtrue;                           /* �Ȃɂ͂Ƃ�����True */
        }
        else                                    /* �O��ON������ */
        {
            if( g_diKeyWait[key] <= g_diKeyCount[key] ) /* �E�F�C�g�^�C���𒴂����� */
            {
                if( g_diKeyInterval[key] != 0 &&
                    (g_diKeyCount[key] - g_diKeyWait[key]) % g_diKeyInterval[key] == 0 ) /* �C���^�[�o���^�C�����Ƃ� */
                {
                    return Qtrue;                           /* True */
                }
            }
        }
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �������u�Ԃ���true�ɂȂ�B�����̓L�[�R�[�h�B
 ---------------------------------------------------------------------*/
static VALUE Input_keyRelease( VALUE obj , VALUE vkey )
{
    int key, padbutton = 0, padbuttonold = 0;

    key = NUM2INT( vkey );
    if( key < 0 || key >= 256 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_keyPush" );
    }

    if( g_diKeyConfig[key] != -1 )
    {
        //if( g_PadState[g_diKeyConfig[key] / 20].button[g_diKeyConfig[key] % 20] == 1 )
        if( g_PadState[g_diKeyConfig[key] / PADBUTTON_MAX].button[g_diKeyConfig[key] % PADBUTTON_MAX] == 1 )
        {
            padbutton = 1;
        }
        //if( g_PadStateOld[g_diKeyConfig[key] / 20].button[g_diKeyConfig[key] % 20] == 1 )
        if( g_PadStateOld[g_diKeyConfig[key] / PADBUTTON_MAX].button[g_diKeyConfig[key] % PADBUTTON_MAX] == 1 )
        {
            padbuttonold = 1;
        }
    }

    if( !(g_diKeyState[key] & 0x80) &&  padbutton == 0 ) /* ���͂���Ă��Ȃ� */
    {
        if( g_diKeyStateOld[key] & 0x80 || padbuttonold == 1 )    /* �O��ON�ł��� */
        {
            return Qtrue;                           /* �Ȃɂ͂Ƃ�����True */
        }
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   ������Ă�����true�ɂȂ�B�����̓{�^���ԍ��B
 ---------------------------------------------------------------------*/
static VALUE Input_padDown( int argc, VALUE *argv, VALUE obj )
{
	VALUE vnumber, vbutton;
	int number, button, key = 0;

    rb_scan_args( argc, argv, "11", &vbutton, &vnumber);

	button = NUM2INT( vbutton );
	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || button < 0 || number >= PADMAX || button >= PADBUTTON_MAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_padDown" );
    }

    if( g_PadState[number].PadConfig[button] != -1 && g_diKeyState[g_PadState[number].PadConfig[button]] & 0x80 )
    {
        key = 1;
    }

    if( key == 1 || g_PadState[number].button[button] == 1 )
    {
        return Qtrue;
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �������u�Ԃ���true�ɂȂ�B�����̓{�^���ԍ��B
 ---------------------------------------------------------------------*/
static VALUE Input_padPush( int argc, VALUE *argv, VALUE obj )
{
	VALUE vnumber, vbutton;
	int number, button, key = 0, keyold = 0;

    rb_scan_args( argc, argv, "11", &vbutton, &vnumber);

	button = NUM2INT( vbutton );
	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || button < 0 || number >= PADMAX || button >= PADBUTTON_MAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_padPush" );
    }

    if( g_PadState[number].PadConfig[button] != -1 )
    {
        if( g_diKeyState[g_PadState[number].PadConfig[button]] & 0x80 )
        {
            key = 1;
        }
        if( g_diKeyStateOld[g_PadState[number].PadConfig[button]] & 0x80 )
        {
            keyold = 1;
        }
    }

    if( key == 1 || g_PadState[number].button[button] == 1 ) /* ���͂���Ă��� */
    {
        if( keyold == 0 && g_PadStateOld[number].button[button] == 0 ) /* �O��OFF�ł��� */
        {
            g_PadState[number].count[button] = 0;                           /* �J�E���g������ */
            return Qtrue;                                                   /* �Ȃɂ͂Ƃ�����True */
        }
        else                                                           /* �O��ON������ */
        {
            if( g_PadState[number].wait[button] <= g_PadState[number].count[button] ) /* �E�F�C�g�^�C���𒴂����� */
            {
                if( g_PadState[number].interval[button] != 0 &&
                   (g_PadState[number].count[button] - g_PadState[number].wait[button])
                    % g_PadState[number].interval[button] == 0 )   /* �C���^�[�o���^�C�����Ƃ� */
                {
                    return Qtrue;                                                           /* True */
                }
            }
        }
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   �������u�Ԃ���true�ɂȂ�B�����̓{�^���ԍ��B
 ---------------------------------------------------------------------*/
static VALUE Input_padRelease( int argc, VALUE *argv, VALUE obj )
{
	VALUE vnumber, vbutton;
	int number, button, key = 0, keyold = 0;

    rb_scan_args( argc, argv, "11", &vbutton, &vnumber);

	button = NUM2INT( vbutton );
	number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

	if( number < 0 || button < 0 || number >= PADMAX || button >= PADBUTTON_MAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_padPush" );
    }

    if( g_PadState[number].PadConfig[button] != -1 )
    {
        if( g_diKeyState[g_PadState[number].PadConfig[button]] & 0x80 )
        {
            key = 1;
        }
        if( g_diKeyStateOld[g_PadState[number].PadConfig[button]] & 0x80 )
        {
            keyold = 1;
        }
    }

    if( key == 0 && g_PadState[number].button[button] == 0 ) /* ���͂���Ă��Ȃ� */
    {
        if( keyold == 1 || g_PadStateOld[number].button[button] == 1 ) /* �O��ON�ł��� */
        {
            return Qtrue;                                                   /* �Ȃɂ͂Ƃ�����True */
        }
    }

    return Qfalse;
}


/*--------------------------------------------------------------------
   �p�b�h�ƃL�[�̑Ή���ݒ肷��
 ---------------------------------------------------------------------*/
static void Input_SetConfig( int number, int pad, int key )
{
    int i, j;

    if( pad == -1 )    /* �L�[�ɑ΂���p�b�h���蓖�ĉ������� */
    {
        g_diKeyConfig[key] = -1;
        for( i = 0; i < PADMAX; i++)
        {
            for( j = 0; j < PADBUTTON_MAX; j++)
            {
                if( g_PadState[i].PadConfig[j] == key )
                {
                    g_PadState[i].PadConfig[j] = -1;
                }
            }
        }
    }
    else if( key == -1 || key == 0 ) /* �p�b�h�ɑ΂���L�[���蓖�ĉ������� */
    {
        g_PadState[number].PadConfig[pad] = -1;
        for( i = 0; i < 256; i++)
        {
            if( g_diKeyConfig[i] == pad )
            {
                g_diKeyConfig[i] = -1;
            }
        }
    }
    else    /* ���蓖�ď��� */
    {
        g_PadState[number].PadConfig[pad] = key;
        //g_diKeyConfig[key] = number * 20 + pad;     //check
        g_diKeyConfig[key] = number * PADBUTTON_MAX + pad;     //check
    }
}

static VALUE Input_setconfig( int argc, VALUE *argv, VALUE obj )
{
    VALUE vnumber, vpad, vkey;
    int number, pad, key;

    rb_scan_args( argc, argv, "21", &vpad, &vkey, &vnumber);

    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));
    pad = (vpad == Qnil ? -1 : NUM2INT( vpad ));
    key = (vkey == Qnil ? -1 : NUM2INT( vkey ));

    if( number < 0 || pad < -1 || key < -1 || number >= PADMAX || pad >= PADBUTTON_MAX || key >= 256 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setconfig" );
    }

    if( pad != -1 || key != -1 )
    {
        Input_SetConfig( number, pad, key );
    }

    return obj;
}


/*--------------------------------------------------------------------
   �}�E�X�z�C�[���̏�Ԃ�Ԃ�
 ---------------------------------------------------------------------*/
static VALUE Input_getmousewheelpos( VALUE obj )
{
    return INT2FIX(g_WindowInfo.mousewheelpos);
}


/*--------------------------------------------------------------------
   �}�E�X�z�C�[���̏�Ԃ�ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Input_setmousewheelpos( VALUE obj, VALUE wheelpos )
{
    g_WindowInfo.mousewheelpos = NUM2INT( wheelpos );

    return obj;
}


/*--------------------------------------------------------------------
   �}�E�X�̃{�^����Ԃ�Ԃ�
 ---------------------------------------------------------------------*/
static VALUE Input_mouseDown( VALUE obj, VALUE button )
{

    switch( NUM2INT( button ) )
    {
    case M_LBUTTON:
        if( g_byMouseState_L & 0x80 )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_RBUTTON:
        if( g_byMouseState_R & 0x80 )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_MBUTTON:
        if( g_byMouseState_M & 0x80 )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }
    default:
        rb_raise( eDXRubyError, "invalid value - Input_mouseDown" );
    }

    return Qnil;
}


/*--------------------------------------------------------------------
   �}�E�X�̃{�^���������ꂽ�Ƃ���true
 ---------------------------------------------------------------------*/
static VALUE Input_mousePush( VALUE obj, VALUE button )
{

    switch( NUM2INT( button ) )
    {
    case M_LBUTTON:
        if( (g_byMouseState_L & 0x80) && !(g_byMouseStateOld_L & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_RBUTTON:
        if( g_byMouseState_R & 0x80 && !(g_byMouseStateOld_R & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_MBUTTON:
        if( g_byMouseState_M & 0x80 && !(g_byMouseStateOld_M & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }
    default:
        rb_raise( eDXRubyError, "invalid value - Input_mousePush" );
    }

    return Qnil;
}


/*--------------------------------------------------------------------
   �}�E�X�̃{�^���������ꂽ�Ƃ���true
 ---------------------------------------------------------------------*/
static VALUE Input_mouseRelease( VALUE obj, VALUE button )
{

    switch( NUM2INT( button ) )
    {
    case M_LBUTTON:
        if( !(g_byMouseState_L & 0x80) && (g_byMouseStateOld_L & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_RBUTTON:
        if( !(g_byMouseState_R & 0x80) && (g_byMouseStateOld_R & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }

    case M_MBUTTON:
        if( !(g_byMouseState_M & 0x80) && (g_byMouseStateOld_M & 0x80) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }
    default:
        rb_raise( eDXRubyError, "invalid value - Input_mouseRelease" );
    }

    return Qnil;
}


/*--------------------------------------------------------------------
   �}�E�X�J�[�\���̈ʒu��Ԃ�
 ---------------------------------------------------------------------*/
static VALUE Input_getmouseposx( VALUE obj )
{
    POINT cursor;

    GetCursorPos( &cursor );
    ScreenToClient( g_hWnd, &cursor );
    return INT2FIX( cursor.x );
}


/*--------------------------------------------------------------------
   �}�E�X�J�[�\���̈ʒu��Ԃ�
 ---------------------------------------------------------------------*/
static VALUE Input_getmouseposy( VALUE obj )
{
    POINT cursor;

    GetCursorPos( &cursor );
    ScreenToClient( g_hWnd, &cursor );

    return INT2FIX( cursor.y );
}


/*--------------------------------------------------------------------
   �}�E�X�J�[�\���̈ʒu��ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Input_setmousepos( VALUE klass, VALUE vx, VALUE vy )
{
    POINT cursor;

    cursor.x = NUM2INT( vx );
    cursor.y = NUM2INT( vy );
    ClientToScreen( g_hWnd, &cursor );
    SetCursorPos( cursor.x, cursor.y );

    return Qnil;
}


/*--------------------------------------------------------------------
   �}�E�X�`�悷�邩�ǂ����̐ݒ�
 ---------------------------------------------------------------------*/
static VALUE Input_enablemouse( VALUE obj, VALUE vdrawflag )
{
    if( RTEST(vdrawflag) )
    {
        ShowCursorMessage();
    }
    else
    {
        HideCursorMessage();
    }

    return vdrawflag;
}


/*--------------------------------------------------------------------
   Input���W���[���̑S�L�[�I�[�g���s�[�g��Ԑݒ�
 ---------------------------------------------------------------------*/
static VALUE Input_setrepeat( VALUE obj , VALUE vwait, VALUE vinterval )
{
    int wait, interval, i, j;

    wait = NUM2INT( vwait );
    interval = NUM2INT( vinterval );

    for( i = 0; i < 256; i++)
    {
        g_diKeyWait[i] = wait;
        g_diKeyInterval[i] = interval;
    }

    for( i = 0; i < PADMAX; i++)
    {
        for( j = 0; j < PADBUTTON_MAX; j++)
        {
            g_PadState[i].wait[j] = wait;
            g_PadState[i].interval[j] = interval;
        }
    }

    return obj;
}


/*--------------------------------------------------------------------
   Input���W���[���̃L�[�I�[�g���s�[�g��Ԑݒ�
 ---------------------------------------------------------------------*/
static VALUE Input_setkeyrepeat( VALUE obj , VALUE vkey, VALUE vwait, VALUE vinterval )
{
    int key, wait, interval;

    key = NUM2INT( vkey );
    wait = NUM2INT( vwait );
    interval = NUM2INT( vinterval );

    if( key < 0 || key >= 256 || wait < 0 || interval < 0 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setKeyRepeat" );
    }

    g_diKeyWait[key] = wait;
    g_diKeyInterval[key] = interval;

    return obj;
}


/*--------------------------------------------------------------------
   Input���W���[���̃p�b�h�I�[�g���s�[�g��Ԑݒ�
 ---------------------------------------------------------------------*/
static VALUE Input_setpadrepeat( int argc, VALUE *argv, VALUE obj )
{
    VALUE vpad, vwait, vinterval, vnumber;
    int pad, wait, interval, number;

    rb_scan_args( argc, argv, "31", &vpad, &vwait, &vinterval, &vnumber );

    pad = NUM2INT( vpad );
    wait = NUM2INT( vwait );
    interval = NUM2INT( vinterval );
    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

    if( number < 0 || pad < 0 || number >= PADMAX || pad >= PADBUTTON_MAX ||
        wait < 0 || interval < 0 )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setPadRepeat" );
    }

    g_PadState[number].wait[pad] = wait;
    g_PadState[number].interval[pad] = interval;

    return obj;
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   ������Ă���L�[�̔z���Ԃ��B
 ---------------------------------------------------------------------*/
static VALUE Input_getKeys( VALUE obj )
{
    int i, j;
    VALUE buf[256];

    for( i = 0, j = 0; i < 256; i++ )
    {
        if( g_diKeyState[i] & 0x80 ) /* ���͂���Ă��� */
        {
            buf[j++] = INT2FIX( i );
        }
    }

    return rb_ary_new4( j, buf );
}


/*--------------------------------------------------------------------
   Input���W���[���̃f�[�^�擾

   ������Ă���p�b�h�̔z���Ԃ��B
 ---------------------------------------------------------------------*/
static VALUE Input_getPads( int argc, VALUE *argv, VALUE obj )
{
    int i, j, number;
    VALUE buf[PADBUTTON_MAX];
    VALUE vnumber;

    rb_scan_args( argc, argv, "01", &vnumber);

    number = vnumber == Qnil ? 0 : NUM2INT( vnumber );

    for( i = 0, j = 0; i < PADBUTTON_MAX; i++ )
    {
        if( g_PadState[number].button[i] == 1 )
        {
            buf[j++] = INT2FIX( i );
        }
    }

    return rb_ary_new4( j, buf );
}


/*--------------------------------------------------------------------
   �p�b�h�̐���Ԃ�
 ---------------------------------------------------------------------*/
static VALUE Input_getPadNum( VALUE obj )
{
    return INT2FIX( g_JoystickCount );
}


/*--------------------------------------------------------------------
   �A�i���O���擾
 ---------------------------------------------------------------------*/
static VALUE Input_getPadAxis( int argc, VALUE *argv, VALUE obj )
{
    VALUE vnumber, vary;
    int number;

    rb_scan_args( argc, argv, "01", &vnumber );

    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

    if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setPadAxis" );
    }

    return rb_ary_new3( 6, rb_float_new( g_PadState[number].x )
                         , rb_float_new( g_PadState[number].y )
                         , rb_float_new( g_PadState[number].z )
                         , rb_float_new( g_PadState[number].rx )
                         , rb_float_new( g_PadState[number].ry )
                         , rb_float_new( g_PadState[number].rz )
                      );
}


/*--------------------------------------------------------------------
   ���X�e�B�b�N�擾
 ---------------------------------------------------------------------*/
static VALUE Input_getPadLStick( int argc, VALUE *argv, VALUE obj )
{
    VALUE vnumber, vary;
    int number;

    rb_scan_args( argc, argv, "01", &vnumber );

    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

    if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setPadAxis" );
    }

    return rb_ary_new3( 2, rb_float_new( g_PadState[number].x )
                         , rb_float_new( g_PadState[number].y )
                      );
}


/*--------------------------------------------------------------------
   �E�X�e�B�b�N�擾
 ---------------------------------------------------------------------*/
static VALUE Input_getPadRStick( int argc, VALUE *argv, VALUE obj )
{
    VALUE vnumber, vary;
    int number;

    rb_scan_args( argc, argv, "01", &vnumber );

    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

    if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setPadAxis" );
    }

    return rb_ary_new3( 2, rb_float_new( g_PadState[number].z )
                         , rb_float_new( g_PadState[number].rz )
                      );
}


/*--------------------------------------------------------------------
   POV�擾
 ---------------------------------------------------------------------*/
static VALUE Input_getPadPov( int argc, VALUE *argv, VALUE obj )
{
    VALUE vnumber, vary;
    int number;

    rb_scan_args( argc, argv, "01", &vnumber );

    number = (vnumber == Qnil ? 0 : NUM2INT( vnumber ));

    if( number < 0 || number >= PADMAX )
    {
        rb_raise( eDXRubyError, "invalid value - Input_setPadAxis" );
    }

    return INT2FIX( g_PadState[number].pov );
}


///*--------------------------------------------------------------------
//   �G�t�F�N�g�J�n
// ---------------------------------------------------------------------*/
//static VALUE Input_ff_start( VALUE obj )
//{
//    g_lpDIEffect->lpVtbl->Start( g_lpDIEffect, 1, 0 );
//    return Qnil;
//}
///*--------------------------------------------------------------------
//   �G�t�F�N�g��~
// ---------------------------------------------------------------------*/
//static VALUE Input_ff_stop( VALUE obj )
//{
//    g_lpDIEffect->lpVtbl->Stop( g_lpDIEffect );
//    return Qnil;
//}


/*--------------------------------------------------------------------
   �J�[�\���`��ύX
 ---------------------------------------------------------------------*/
static VALUE Input_setCursor( VALUE obj , VALUE vcursor )
{

    SetCursorMessage( NUM2INT(vcursor) );
    return vcursor;
}


#ifdef DXRUBY15
/*--------------------------------------------------------------------
   IME���������
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getstring( VALUE obj )
{
    return rb_str_export_to_enc( rb_enc_str_new( (char*)ime_buf_old, ime_str_length_old * 2, g_enc_utf16 ), g_enc_utf8 );
}

UINT DI_KeyList[][2] = { {VK_UP,DIK_UP}, {VK_DOWN,DIK_DOWN}, {VK_LEFT,DIK_LEFT}, {VK_RIGHT,DIK_RIGHT},
 {VK_INSERT,DIK_INSERT}, {VK_HOME,DIK_HOME}, {VK_PRIOR,DIK_PRIOR},
 {VK_DELETE,DIK_DELETE}, {VK_END,DIK_END}, {VK_NEXT,DIK_NEXT},
 {VK_LMENU,DIK_LMENU}, {VK_RMENU,DIK_RMENU}, {VK_LCONTROL,DIK_LCONTROL}, {VK_RCONTROL,DIK_RCONTROL},
 {VK_DIVIDE,DIK_DIVIDE} };

/*--------------------------------------------------------------------
   IME�������L�[�ꗗ
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getpushkeys( VALUE obj )
{
    int i, j;
    VALUE ary;

    if( ime_vk_push_str_length_old == 0 )
    {
        return rb_ary_new();
    }

    ary = rb_ary_new2( ime_vk_push_str_length_old );

    for( i = 0; i < ime_vk_push_str_length_old ; i++ )
    {
        for( j = 0; j < sizeof(DI_KeyList)/(sizeof(UINT)*2); j++ )
        {
            if( ime_vk_push_buf_old[i] == DI_KeyList[j][0] )
            {
                rb_ary_push( ary, INT2FIX( DI_KeyList[j][1] ));
                break;
            }
        }

        if( j == sizeof(DI_KeyList)/(sizeof(UINT)*2) )
        {
            rb_ary_push( ary, INT2FIX(MapVirtualKey( ime_vk_push_buf_old[i], 0 )));
        }
    }

    return ary;
}


/*--------------------------------------------------------------------
   IME�������L�[�ꗗ
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getreleasekeys( VALUE obj )
{
    int i, j;
    VALUE ary;

    if( ime_vk_release_str_length_old == 0 )
    {
        return rb_ary_new();
    }

    ary = rb_ary_new2( ime_vk_release_str_length_old );

    for( i = 0; i < ime_vk_release_str_length_old ; i++ )
    {
        for( j = 0; j < sizeof(DI_KeyList)/(sizeof(UINT)*2); j++ )
        {
            if( ime_vk_release_buf_old[i] == DI_KeyList[j][0] )
            {
                rb_ary_push( ary, INT2FIX( DI_KeyList[j][1] ));
                break;
            }
        }

        if( j == sizeof(DI_KeyList)/(sizeof(UINT)*2) )
        {
            rb_ary_push( ary, INT2FIX(MapVirtualKey( ime_vk_release_buf_old[i], 0 )));
        }
    }

    return ary;
}


/*--------------------------------------------------------------------
   IME��enable
 ---------------------------------------------------------------------*/
static VALUE Input_IME_setenable( VALUE obj, VALUE vflag )
{
    SetImeEnable( RTEST( vflag ) );
    return vflag;
}


/*--------------------------------------------------------------------
   IME��enable�擾
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getenable( VALUE obj )
{
    return ( GetImeEnable() ? Qtrue : Qfalse );
}


/*--------------------------------------------------------------------
   IME�̏�Ԏ擾
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getcompositing( VALUE obj )
{
    return ( ime_compositing ? Qtrue : Qfalse );
}

/*--------------------------------------------------------------------
   IME�̕ϊ���������擾
 ---------------------------------------------------------------------*/
static VALUE Input_IME_getcompositioninfo( VALUE obj )
{
    VALUE ary[8];
    ary[0] = RARRAY_AREF( g_vCompInfoArray, 0 );
    ary[1] = RARRAY_AREF( g_vCompInfoArray, 1 );
    ary[2] = RARRAY_AREF( g_vCompInfoArray, 2 );
    ary[3] = RARRAY_AREF( g_vCompInfoArray, 3 );
    ary[4] = RARRAY_AREF( g_vCompInfoArray, 4 );
    ary[5] = RARRAY_AREF( g_vCompInfoArray, 5 );
    ary[6] = RARRAY_AREF( g_vCompInfoArray, 6 );
    ary[7] = RARRAY_AREF( g_vCompInfoArray, 7 );
    return rb_class_new_instance( 8, ary, cCompInfo );
}
#endif


/* �f�o�C�X�񋓊֐� */
BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance, void* pContext )
{
    HRESULT hr;

    /* �񋓂��ꂽ�W���C�X�e�B�b�N�ւ̃C���^�[�t�F�C�X���擾���� */
    hr = g_pDInput->lpVtbl->CreateDevice( g_pDInput,
                            &pdidInstance->guidInstance,
                            &g_pDIDJoyPad[g_JoystickCount],
                            NULL );
    if( SUCCEEDED( hr ) )
    {
        return (++g_JoystickCount == PADMAX)?(DIENUM_STOP):(DIENUM_CONTINUE);
    }

    return DIENUM_STOP;
}

/* �I�u�W�F�N�g�i���j�񋓊֐� */
BOOL CALLBACK EnumAxisCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, void *pContext )
{
    HRESULT hr;
    DIPROPRANGE diprg = { sizeof(DIPROPRANGE), sizeof(DIPROPHEADER) };
    struct DXRubyPadInfo *pad;

    pad = (struct DXRubyPadInfo *)pContext;

    diprg.diph.dwHow = DIPH_BYID; 
    diprg.diph.dwObj = pdidoi->dwType;

    /*========================================================*/
    /* �擾 */
    hr =  pad->pDIDJoyPad->lpVtbl->GetProperty( pad->pDIDJoyPad, DIPROP_RANGE, &diprg.diph );

    if( FAILED( hr ) )
    {
        MainThreadError = 17;
        return DIENUM_STOP;
    }

    /* x�� */
    if( memcmp( &pdidoi->guidType, &GUID_XAxis ,sizeof(GUID)) == 0 )
    {
        pad->x_flg = 1;
        pad->x_min = diprg.lMin;
        pad->x_center = (diprg.lMin + diprg.lMax) / 2;
        pad->x_max = diprg.lMax;
        if ( ( pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0 )
        {
            pad->x_ff_flg = 1;
        }
    }

    /* y�� */
    if( memcmp( &pdidoi->guidType, &GUID_YAxis ,sizeof(GUID)) == 0 )
    {
        pad->y_flg = 1;
        pad->y_min = diprg.lMin;
        pad->y_center = (diprg.lMin + diprg.lMax) / 2;
        pad->y_max = diprg.lMax;
        if ( ( pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0 )
        {
            pad->y_ff_flg = 1;
        }
    }

    /* z�� */
    if( memcmp( &pdidoi->guidType, &GUID_ZAxis ,sizeof(GUID)) == 0 )
    {
        pad->z_flg = 1;
        pad->z_min = diprg.lMin;
        pad->z_center = (diprg.lMin + diprg.lMax) / 2;
        pad->z_max = diprg.lMax;
    }

    /* x����] */
    if( memcmp( &pdidoi->guidType, &GUID_RxAxis ,sizeof(GUID)) == 0 )
    {
        pad->rx_flg = 1;
        pad->rx_min = diprg.lMin;
        pad->rx_center = (diprg.lMin + diprg.lMax) / 2;
        pad->rx_max = diprg.lMax;
    }

    /* y����] */
    if( memcmp( &pdidoi->guidType, &GUID_RyAxis ,sizeof(GUID)) == 0 )
    {
        pad->ry_flg = 1;
        pad->ry_min = diprg.lMin;
        pad->ry_center = (diprg.lMin + diprg.lMax) / 2;
        pad->ry_max = diprg.lMax;
    }

    /* z����] */
    if( memcmp( &pdidoi->guidType, &GUID_RzAxis ,sizeof(GUID)) == 0 )
    {
        pad->rz_flg = 1;
        pad->rz_min = diprg.lMin;
        pad->rz_center = (diprg.lMin + diprg.lMax) / 2;
        pad->rz_max = diprg.lMax;
    }

    return DIENUM_CONTINUE;
}

/*--------------------------------------------------------------------
  �i�����֐��jDirectInput������
 ---------------------------------------------------------------------*/
int InitDirectInput( void )
{
    HRESULT hr;
    int i;

/* DirectInput������ */

    /* DirectInput�I�u�W�F�N�g�̍쐬 */
    hr = DirectInput8Create( g_hInstance, DIRECTINPUT_VERSION,
                             &IID_IDirectInput8, (void **)&g_pDInput, NULL );

    if( FAILED( hr ) )
    {
        MainThreadError = 10;
        return MainThreadError;
    }

/* �L�[�{�[�h */

    /* �f�o�C�X�E�I�u�W�F�N�g���쐬�i�L�[�{�[�h�j */
    hr = g_pDInput->lpVtbl->CreateDevice( g_pDInput, &GUID_SysKeyboard, &g_pDIDKeyBoard, NULL );

    if( FAILED( hr ) )
    {
        MainThreadError = 11;
        return MainThreadError;
    }

    /* �f�[�^�`����ݒ�i�L�[�{�[�h�ł���j */
    hr = g_pDIDKeyBoard->lpVtbl->SetDataFormat( g_pDIDKeyBoard, &c_dfDIKeyboard );

    if( FAILED( hr ) )
    {
        MainThreadError = 12;
        return MainThreadError;
    }

    /* �L�[�{�[�h�̃��[�h��ݒ�i�t�H�A�O���E���h����r�����[�h�j */
    hr = g_pDIDKeyBoard->lpVtbl->SetCooperativeLevel( g_pDIDKeyBoard, g_hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND );

    if( FAILED( hr ) )
    {
        MainThreadError = 13;
        return MainThreadError;
    }

    /* ���͐���J�n */
    g_pDIDKeyBoard->lpVtbl->Acquire( g_pDIDKeyBoard );


/* �Q�[���p�b�h */

    for( i = 0; i < PADMAX; i++)
    {
        g_pDIDJoyPad[i] = NULL;
    }
    g_JoystickCount = 0;

    /* �Q�[���f�o�C�X�� */
    hr = g_pDInput->lpVtbl->EnumDevices( g_pDInput, DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY );

    if( MainThreadError != 0 )
    {
        return MainThreadError;
    }

    if( FAILED( hr ) )
    {
        MainThreadError = 14;
        return MainThreadError;
    }

    /* �Q�[���f�o�C�X������ */
    for ( i = 0; i < g_JoystickCount; i++ )
    {
        /* �f�[�^�`����ݒ� */
        hr = g_pDIDJoyPad[i]->lpVtbl->SetDataFormat( g_pDIDJoyPad[i], &c_dfDIJoystick );

        if( FAILED( hr ) )
        {
            MainThreadError = 15;
            return MainThreadError;
        }

        /* �L�[�{�[�h�̃��[�h��ݒ�i�t�H�A�O���E���h����r�����[�h�j */
        hr = g_pDIDJoyPad[i]->lpVtbl->SetCooperativeLevel( g_pDIDJoyPad[i], g_hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND );

        if( FAILED( hr ) )
        {
            MainThreadError = 16;
            return MainThreadError;
        }

        /* ���I�u�W�F�N�g�� */
        g_PadInfo[i].pDIDJoyPad = g_pDIDJoyPad[i];
        g_PadInfo[i].x_flg  = g_PadInfo[i].y_flg  = g_PadInfo[i].z_flg = 
        g_PadInfo[i].rx_flg = g_PadInfo[i].ry_flg = g_PadInfo[i].rz_flg = 
        g_PadInfo[i].x_ff_flg = g_PadInfo[i].y_ff_flg = 0;
        hr = g_pDIDJoyPad[i]->lpVtbl->EnumObjects( g_pDIDJoyPad[i], EnumAxisCallback, &g_PadInfo[i], DIDFT_AXIS );

        if( MainThreadError != 0 )
        {
            return MainThreadError;
        }

        if( FAILED( hr ) )
        {
            MainThreadError = 18;
            return MainThreadError;
        }

//        /* �t�H�[�X�t�B�[�h�o�b�N���� */
//        if( g_PadInfo[i].x_ff_flg + g_PadInfo[i].y_ff_flg > 0 )
//        {
//
//            DWORD           rgdwAxes[2]     = { DIJOFS_X , DIJOFS_Y };
//            LONG            rglDirection[2] = { 1 , 1 };
//            DICONSTANTFORCE cf;
//            DIEFFECT        eff;
//            HRESULT         hr;
//
//            ZeroMemory( &eff , sizeof( eff ) );
//            eff.dwSize                  = sizeof( DIEFFECT );
//            eff.dwFlags                 = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
//            eff.dwDuration              = INFINITE;
//            eff.dwSamplePeriod          = 0;
//            eff.dwGain                  = DI_FFNOMINALMAX;
//            eff.dwTriggerButton         = DIEB_NOTRIGGER;
//            eff.dwTriggerRepeatInterval = 0;
//            eff.cAxes                   = g_PadInfo[i].x_ff_flg + g_PadInfo[i].y_ff_flg;
//            eff.rgdwAxes                = rgdwAxes;
//            eff.rglDirection            = rglDirection;
//            eff.lpEnvelope              = 0;
//            eff.cbTypeSpecificParams    = sizeof( DICONSTANTFORCE );
//            eff.lpvTypeSpecificParams   = &cf;
//            eff.dwStartDelay            = 0;
//
//            hr = g_pDIDJoyPad[i]->lpVtbl->CreateEffect( g_pDIDJoyPad[i], &GUID_ConstantForce, &eff, &g_lpDIEffect, NULL );
//            if ( FAILED( hr ) ){
//                rb_raise( eDXRubyError, "DirectInput�̏������Ɏ��s���܂��� - CreateEffect" );
//            }
//
//        }

        /* ���͐���J�n */
        g_pDIDJoyPad[i]->lpVtbl->Acquire( g_pDIDJoyPad[i] );
    }

    return 0;
}


void Input_release( void )
{
    int i;

//    g_lpDIEffect->lpVtbl->Unload(g_lpDIEffect);
//    RELEASE( g_lpDIEffect );

    for( i = 0 ; i < g_JoystickCount; i++ )
    {
        /* DirectInputDevice(JoyPad)�I�u�W�F�N�g�̎g�p�I�� */
        if( g_pDIDJoyPad[i] )
        {
            g_pDIDJoyPad[i]->lpVtbl->Unacquire( g_pDIDJoyPad[i] ); 
        }

        /* DirectInputDevide(JoyPad)�I�u�W�F�N�g�̔j�� */
        RELEASE( g_pDIDJoyPad[i] );
    }

    /* DirectInputDevice(Keyboard)�I�u�W�F�N�g�̎g�p�I�� */
    if( g_pDIDKeyBoard )
    {
           g_pDIDKeyBoard->lpVtbl->Unacquire( g_pDIDKeyBoard ); 
    }

    /* DirectInputDevide(Keyboard)�I�u�W�F�N�g�̔j�� */
    RELEASE( g_pDIDKeyBoard );

    /* DirectInput�I�u�W�F�N�g�̔j�� */
    RELEASE( g_pDInput );
}

void Init_dxruby_Input( void )
{
    int i, j;

    /* Input���W���[���o�^ */
    mInput = rb_define_module_under( mDXRuby, "Input" );

    /* Input���W���[���Ƀ��\�b�h�o�^ */
    rb_define_singleton_method( mInput, "x"              , Input_x               , -1 );
    rb_define_singleton_method( mInput, "y"              , Input_y               , -1 );
    rb_define_singleton_method( mInput, "set_mouse_pos"  , Input_setmousepos     , 2  );
    rb_define_singleton_method( mInput, "setMousePos"    , Input_setmousepos     , 2  );
    rb_define_singleton_method( mInput, "mouse_pos_x"    , Input_getmouseposx    , 0  );
    rb_define_singleton_method( mInput, "mousePosX"      , Input_getmouseposx    , 0  );
    rb_define_singleton_method( mInput, "mouse_pos_y"    , Input_getmouseposy    , 0  );
    rb_define_singleton_method( mInput, "mousePosY"      , Input_getmouseposy    , 0  );
    rb_define_singleton_method( mInput, "mouse_x"        , Input_getmouseposx    , 0  );
    rb_define_singleton_method( mInput, "mouseX"         , Input_getmouseposx    , 0  );
    rb_define_singleton_method( mInput, "mouse_y"        , Input_getmouseposy    , 0  );
    rb_define_singleton_method( mInput, "mouseY"         , Input_getmouseposy    , 0  );
    rb_define_singleton_method( mInput, "key_down?"      , Input_keyDown         , 1  );
    rb_define_singleton_method( mInput, "keyDown?"       , Input_keyDown         , 1  );
    rb_define_singleton_method( mInput, "key_push?"      , Input_keyPush         , 1  );
    rb_define_singleton_method( mInput, "keyPush?"       , Input_keyPush         , 1  );
    rb_define_singleton_method( mInput, "key_release?"   , Input_keyRelease      , 1  );
    rb_define_singleton_method( mInput, "keyRelease?"    , Input_keyRelease      , 1  );
    rb_define_singleton_method( mInput, "pad_down?"      , Input_padDown         , -1 );
    rb_define_singleton_method( mInput, "padDown?"       , Input_padDown         , -1 );
    rb_define_singleton_method( mInput, "pad_push?"      , Input_padPush         , -1 );
    rb_define_singleton_method( mInput, "padPush?"       , Input_padPush         , -1 );
    rb_define_singleton_method( mInput, "pad_release?"   , Input_padRelease      , -1 );
    rb_define_singleton_method( mInput, "padRelease?"    , Input_padRelease      , -1 );
    rb_define_singleton_method( mInput, "mouse_down?"    , Input_mouseDown       , 1  );
    rb_define_singleton_method( mInput, "mouseDown?"     , Input_mouseDown       , 1  );
    rb_define_singleton_method( mInput, "mouse_push?"    , Input_mousePush       , 1  );
    rb_define_singleton_method( mInput, "mousePush?"     , Input_mousePush       , 1  );
    rb_define_singleton_method( mInput, "mouse_release?" , Input_mouseRelease    , 1  );
    rb_define_singleton_method( mInput, "mouseRelease?"  , Input_mouseRelease    , 1  );
    rb_define_singleton_method( mInput, "mouse_enable="  , Input_enablemouse     , 1  );
    rb_define_singleton_method( mInput, "mouseEnable="   , Input_enablemouse     , 1  );
    rb_define_singleton_method( mInput, "mouse_wheel_pos", Input_getmousewheelpos, 0  );
    rb_define_singleton_method( mInput, "mouseWheelPos"  , Input_getmousewheelpos, 0  );
    rb_define_singleton_method( mInput, "mouse_wheel_pos=", Input_setmousewheelpos, 1  );
    rb_define_singleton_method( mInput, "mouseWheelPos=" , Input_setmousewheelpos, 1  );
    rb_define_singleton_method( mInput, "set_config"     , Input_setconfig       , -1 );
    rb_define_singleton_method( mInput, "setConfig"      , Input_setconfig       , -1 );
    rb_define_singleton_method( mInput, "set_repeat"     , Input_setrepeat       , 2  );
    rb_define_singleton_method( mInput, "setRepeat"      , Input_setrepeat       , 2  );
    rb_define_singleton_method( mInput, "update"         , Input_update          , 0  );
    rb_define_singleton_method( mInput, "set_key_repeat" , Input_setkeyrepeat    , 3  );
    rb_define_singleton_method( mInput, "setKeyRepeat"   , Input_setkeyrepeat    , 3  );
    rb_define_singleton_method( mInput, "set_pad_repeat" , Input_setpadrepeat    , -1 );
    rb_define_singleton_method( mInput, "setPadRepeat"   , Input_setpadrepeat    , -1 );
    rb_define_singleton_method( mInput, "keys"           , Input_getKeys         , 0  );
    rb_define_singleton_method( mInput, "pads"           , Input_getPads         , -1 );
    rb_define_singleton_method( mInput, "pad_num"        , Input_getPadNum       , 0  );
    rb_define_singleton_method( mInput, "requested_close?", Input_requested_close, 0  );

    rb_define_singleton_method( mInput, "pad_axis"       , Input_getPadAxis      , -1 );
    rb_define_singleton_method( mInput, "pad_lstick"     , Input_getPadLStick    , -1 );
    rb_define_singleton_method( mInput, "pad_rstick"     , Input_getPadRStick    , -1 );
    rb_define_singleton_method( mInput, "pad_pov"        , Input_getPadPov       , -1 );
//    rb_define_singleton_method( mInput, "start"        , Input_ff_start        , 0 );
//    rb_define_singleton_method( mInput, "stop"        , Input_ff_stop        , 0 );

    /* �E�X�e�B�b�N�Ə\���{�^���̃��\�b�h�ǉ�*/
    rb_define_singleton_method( mInput, "pad_lx"             , Input_lx              , -1 );
    rb_define_singleton_method( mInput, "pad_ly"             , Input_ly              , -1 );
    rb_define_singleton_method( mInput, "pad_rx"             , Input_rx              , -1 );
    rb_define_singleton_method( mInput, "pad_ry"             , Input_ry              , -1 );
    rb_define_singleton_method( mInput, "pad_pov_x"          , Input_dx              , -1 );
    rb_define_singleton_method( mInput, "pad_pov_y"          , Input_dy              , -1 );
    rb_define_singleton_method( mInput, "set_cursor"         , Input_setCursor       , 1 );


    /* �L�[�{�[�h�̃X�L�����R�[�h�萔�ݒ� */
    rb_define_const( mDXRuby, "K_ESCAPE"     , INT2FIX(DIK_ESCAPE) );
    rb_define_const( mDXRuby, "K_1"          , INT2FIX(DIK_1) );
    rb_define_const( mDXRuby, "K_2"          , INT2FIX(DIK_2) );
    rb_define_const( mDXRuby, "K_3"          , INT2FIX(DIK_3) );
    rb_define_const( mDXRuby, "K_4"          , INT2FIX(DIK_4) );
    rb_define_const( mDXRuby, "K_5"          , INT2FIX(DIK_5) );
    rb_define_const( mDXRuby, "K_6"          , INT2FIX(DIK_6) );
    rb_define_const( mDXRuby, "K_7"          , INT2FIX(DIK_7) );
    rb_define_const( mDXRuby, "K_8"          , INT2FIX(DIK_8) );
    rb_define_const( mDXRuby, "K_9"          , INT2FIX(DIK_9) );
    rb_define_const( mDXRuby, "K_0"          , INT2FIX(DIK_0) );
    rb_define_const( mDXRuby, "K_MINUS"      , INT2FIX(DIK_MINUS) );
    rb_define_const( mDXRuby, "K_EQUALS"     , INT2FIX(DIK_EQUALS) );
    rb_define_const( mDXRuby, "K_BACK"       , INT2FIX(DIK_BACK) );
    rb_define_const( mDXRuby, "K_TAB"        , INT2FIX(DIK_TAB) );
    rb_define_const( mDXRuby, "K_Q"          , INT2FIX(DIK_Q) );
    rb_define_const( mDXRuby, "K_W"          , INT2FIX(DIK_W) );
    rb_define_const( mDXRuby, "K_E"          , INT2FIX(DIK_E) );
    rb_define_const( mDXRuby, "K_R"          , INT2FIX(DIK_R) );
    rb_define_const( mDXRuby, "K_T"          , INT2FIX(DIK_T) );
    rb_define_const( mDXRuby, "K_Y"          , INT2FIX(DIK_Y) );
    rb_define_const( mDXRuby, "K_U"          , INT2FIX(DIK_U) );
    rb_define_const( mDXRuby, "K_I"          , INT2FIX(DIK_I) );
    rb_define_const( mDXRuby, "K_O"          , INT2FIX(DIK_O) );
    rb_define_const( mDXRuby, "K_P"          , INT2FIX(DIK_P) );
    rb_define_const( mDXRuby, "K_LBRACKET"   , INT2FIX(DIK_LBRACKET) );
    rb_define_const( mDXRuby, "K_RBRACKET"   , INT2FIX(DIK_RBRACKET) );
    rb_define_const( mDXRuby, "K_RETURN"     , INT2FIX(DIK_RETURN) );
    rb_define_const( mDXRuby, "K_LCONTROL"   , INT2FIX(DIK_LCONTROL) );
    rb_define_const( mDXRuby, "K_A"          , INT2FIX(DIK_A) );
    rb_define_const( mDXRuby, "K_S"          , INT2FIX(DIK_S) );
    rb_define_const( mDXRuby, "K_D"          , INT2FIX(DIK_D) );
    rb_define_const( mDXRuby, "K_F"          , INT2FIX(DIK_F) );
    rb_define_const( mDXRuby, "K_G"          , INT2FIX(DIK_G) );
    rb_define_const( mDXRuby, "K_H"          , INT2FIX(DIK_H) );
    rb_define_const( mDXRuby, "K_J"          , INT2FIX(DIK_J) );
    rb_define_const( mDXRuby, "K_K"          , INT2FIX(DIK_K) );
    rb_define_const( mDXRuby, "K_L"          , INT2FIX(DIK_L) );
    rb_define_const( mDXRuby, "K_SEMICOLON"  , INT2FIX(DIK_SEMICOLON) );
    rb_define_const( mDXRuby, "K_APOSTROPHE" , INT2FIX(DIK_APOSTROPHE) );
    rb_define_const( mDXRuby, "K_GRAVE"      , INT2FIX(DIK_GRAVE) );
    rb_define_const( mDXRuby, "K_LSHIFT"     , INT2FIX(DIK_LSHIFT) );
    rb_define_const( mDXRuby, "K_BACKSLASH"  , INT2FIX(DIK_BACKSLASH) );
    rb_define_const( mDXRuby, "K_Z"          , INT2FIX(DIK_Z) );
    rb_define_const( mDXRuby, "K_X"          , INT2FIX(DIK_X) );
    rb_define_const( mDXRuby, "K_C"          , INT2FIX(DIK_C) );
    rb_define_const( mDXRuby, "K_V"          , INT2FIX(DIK_V) );
    rb_define_const( mDXRuby, "K_B"          , INT2FIX(DIK_B) );
    rb_define_const( mDXRuby, "K_N"          , INT2FIX(DIK_N) );
    rb_define_const( mDXRuby, "K_M"          , INT2FIX(DIK_M) );
    rb_define_const( mDXRuby, "K_COMMA"      , INT2FIX(DIK_COMMA) );
    rb_define_const( mDXRuby, "K_PERIOD"     , INT2FIX(DIK_PERIOD) );
    rb_define_const( mDXRuby, "K_SLASH"      , INT2FIX(DIK_SLASH) );
    rb_define_const( mDXRuby, "K_RSHIFT"     , INT2FIX(DIK_RSHIFT) );
    rb_define_const( mDXRuby, "K_MULTIPLY"   , INT2FIX(DIK_MULTIPLY) );
    rb_define_const( mDXRuby, "K_LMENU"      , INT2FIX(DIK_LMENU) );
    rb_define_const( mDXRuby, "K_SPACE"      , INT2FIX(DIK_SPACE) );
    rb_define_const( mDXRuby, "K_CAPITAL"    , INT2FIX(DIK_CAPITAL) );
    rb_define_const( mDXRuby, "K_F1"         , INT2FIX(DIK_F1) );
    rb_define_const( mDXRuby, "K_F2"         , INT2FIX(DIK_F2) );
    rb_define_const( mDXRuby, "K_F3"         , INT2FIX(DIK_F3) );
    rb_define_const( mDXRuby, "K_F4"         , INT2FIX(DIK_F4) );
    rb_define_const( mDXRuby, "K_F5"         , INT2FIX(DIK_F5) );
    rb_define_const( mDXRuby, "K_F6"         , INT2FIX(DIK_F6) );
    rb_define_const( mDXRuby, "K_F7"         , INT2FIX(DIK_F7) );
    rb_define_const( mDXRuby, "K_F8"         , INT2FIX(DIK_F8) );
    rb_define_const( mDXRuby, "K_F9"         , INT2FIX(DIK_F9) );
    rb_define_const( mDXRuby, "K_F10"        , INT2FIX(DIK_F10) );
    rb_define_const( mDXRuby, "K_NUMLOCK"    , INT2FIX(DIK_NUMLOCK) );
    rb_define_const( mDXRuby, "K_SCROLL"     , INT2FIX(DIK_SCROLL) );
    rb_define_const( mDXRuby, "K_NUMPAD7"    , INT2FIX(DIK_NUMPAD7) );
    rb_define_const( mDXRuby, "K_NUMPAD8"    , INT2FIX(DIK_NUMPAD8) );
    rb_define_const( mDXRuby, "K_NUMPAD9"    , INT2FIX(DIK_NUMPAD9) );
    rb_define_const( mDXRuby, "K_SUBTRACT"   , INT2FIX(DIK_SUBTRACT) );
    rb_define_const( mDXRuby, "K_NUMPAD4"    , INT2FIX(DIK_NUMPAD4) );
    rb_define_const( mDXRuby, "K_NUMPAD5"    , INT2FIX(DIK_NUMPAD5) );
    rb_define_const( mDXRuby, "K_NUMPAD6"    , INT2FIX(DIK_NUMPAD6) );
    rb_define_const( mDXRuby, "K_ADD"        , INT2FIX(DIK_ADD) );
    rb_define_const( mDXRuby, "K_NUMPAD1"    , INT2FIX(DIK_NUMPAD1) );
    rb_define_const( mDXRuby, "K_NUMPAD2"    , INT2FIX(DIK_NUMPAD2) );
    rb_define_const( mDXRuby, "K_NUMPAD3"    , INT2FIX(DIK_NUMPAD3) );
    rb_define_const( mDXRuby, "K_NUMPAD0"    , INT2FIX(DIK_NUMPAD0) );
    rb_define_const( mDXRuby, "K_DECIMAL"    , INT2FIX(DIK_DECIMAL) );
    rb_define_const( mDXRuby, "K_OEM_102"    , INT2FIX(DIK_OEM_102) );
    rb_define_const( mDXRuby, "K_F11"        , INT2FIX(DIK_F11) );
    rb_define_const( mDXRuby, "K_F12"        , INT2FIX(DIK_F12) );
    rb_define_const( mDXRuby, "K_F13"        , INT2FIX(DIK_F13) );
    rb_define_const( mDXRuby, "K_F14"        , INT2FIX(DIK_F14) );
    rb_define_const( mDXRuby, "K_F15"        , INT2FIX(DIK_F15) );
    rb_define_const( mDXRuby, "K_KANA"       , INT2FIX(DIK_KANA) );
    rb_define_const( mDXRuby, "K_ABNT_C1"    , INT2FIX(DIK_ABNT_C1) );
    rb_define_const( mDXRuby, "K_CONVERT"    , INT2FIX(DIK_CONVERT) );
    rb_define_const( mDXRuby, "K_NOCONVERT"  , INT2FIX(DIK_NOCONVERT) );
    rb_define_const( mDXRuby, "K_YEN"        , INT2FIX(DIK_YEN) );
    rb_define_const( mDXRuby, "K_ABNT_C2"    , INT2FIX(DIK_ABNT_C2) );
    rb_define_const( mDXRuby, "K_NUMPADEQUALS", INT2FIX(DIK_NUMPADEQUALS) );
    rb_define_const( mDXRuby, "K_PREVTRACK"  , INT2FIX(DIK_PREVTRACK) );
    rb_define_const( mDXRuby, "K_AT"         , INT2FIX(DIK_AT) );
    rb_define_const( mDXRuby, "K_COLON"      , INT2FIX(DIK_COLON) );
    rb_define_const( mDXRuby, "K_UNDERLINE"  , INT2FIX(DIK_UNDERLINE) );
    rb_define_const( mDXRuby, "K_KANJI"      , INT2FIX(DIK_KANJI) );
    rb_define_const( mDXRuby, "K_STOP"       , INT2FIX(DIK_STOP) );
    rb_define_const( mDXRuby, "K_AX"         , INT2FIX(DIK_AX) );
    rb_define_const( mDXRuby, "K_UNLABELED"  , INT2FIX(DIK_UNLABELED) );
    rb_define_const( mDXRuby, "K_NEXTTRACK"  , INT2FIX(DIK_NEXTTRACK) );
    rb_define_const( mDXRuby, "K_NUMPADENTER", INT2FIX(DIK_NUMPADENTER) );
    rb_define_const( mDXRuby, "K_RCONTROL"   , INT2FIX(DIK_RCONTROL) );
    rb_define_const( mDXRuby, "K_MUTE"       , INT2FIX(DIK_MUTE) );
    rb_define_const( mDXRuby, "K_CALCULATOR" , INT2FIX(DIK_CALCULATOR) );
    rb_define_const( mDXRuby, "K_PLAYPAUSE"  , INT2FIX(DIK_PLAYPAUSE) );
    rb_define_const( mDXRuby, "K_MEDIASTOP"  , INT2FIX(DIK_MEDIASTOP) );
    rb_define_const( mDXRuby, "K_VOLUMEDOWN" , INT2FIX(DIK_VOLUMEDOWN) );
    rb_define_const( mDXRuby, "K_VOLUMEUP"   , INT2FIX(DIK_VOLUMEUP) );
    rb_define_const( mDXRuby, "K_WEBHOME"    , INT2FIX(DIK_WEBHOME) );
    rb_define_const( mDXRuby, "K_NUMPADCOMMA", INT2FIX(DIK_NUMPADCOMMA) );
    rb_define_const( mDXRuby, "K_DIVIDE"     , INT2FIX(DIK_DIVIDE) );
    rb_define_const( mDXRuby, "K_SYSRQ"      , INT2FIX(DIK_SYSRQ) );
    rb_define_const( mDXRuby, "K_RMENU"      , INT2FIX(DIK_RMENU) );
    rb_define_const( mDXRuby, "K_PAUSE"      , INT2FIX(DIK_PAUSE) );
    rb_define_const( mDXRuby, "K_HOME"       , INT2FIX(DIK_HOME) );
    rb_define_const( mDXRuby, "K_UP"         , INT2FIX(DIK_UP) );
    rb_define_const( mDXRuby, "K_PRIOR"      , INT2FIX(DIK_PRIOR) );
    rb_define_const( mDXRuby, "K_LEFT"       , INT2FIX(DIK_LEFT) );
    rb_define_const( mDXRuby, "K_RIGHT"      , INT2FIX(DIK_RIGHT) );
    rb_define_const( mDXRuby, "K_END"        , INT2FIX(DIK_END) );
    rb_define_const( mDXRuby, "K_DOWN"       , INT2FIX(DIK_DOWN) );
    rb_define_const( mDXRuby, "K_NEXT"       , INT2FIX(DIK_NEXT) );
    rb_define_const( mDXRuby, "K_INSERT"     , INT2FIX(DIK_INSERT) );
    rb_define_const( mDXRuby, "K_DELETE"     , INT2FIX(DIK_DELETE) );
    rb_define_const( mDXRuby, "K_LWIN"       , INT2FIX(DIK_LWIN) );
    rb_define_const( mDXRuby, "K_RWIN"       , INT2FIX(DIK_RWIN) );
    rb_define_const( mDXRuby, "K_APPS"       , INT2FIX(DIK_APPS) );
    rb_define_const( mDXRuby, "K_POWER"      , INT2FIX(DIK_POWER) );
    rb_define_const( mDXRuby, "K_SLEEP"      , INT2FIX(DIK_SLEEP) );
    rb_define_const( mDXRuby, "K_WAKE"       , INT2FIX(DIK_WAKE) );
    rb_define_const( mDXRuby, "K_WEBSEARCH"  , INT2FIX(DIK_WEBSEARCH) );
    rb_define_const( mDXRuby, "K_WEBFAVORITES", INT2FIX(DIK_WEBFAVORITES) );
    rb_define_const( mDXRuby, "K_WEBREFRESH" , INT2FIX(DIK_WEBREFRESH) );
    rb_define_const( mDXRuby, "K_WEBSTOP"    , INT2FIX(DIK_WEBSTOP) );
    rb_define_const( mDXRuby, "K_WEBFORWARD" , INT2FIX(DIK_WEBFORWARD) );
    rb_define_const( mDXRuby, "K_WEBBACK"    , INT2FIX(DIK_WEBBACK) );
    rb_define_const( mDXRuby, "K_MYCOMPUTER" , INT2FIX(DIK_MYCOMPUTER) );
    rb_define_const( mDXRuby, "K_MAIL"       , INT2FIX(DIK_MAIL) );
    rb_define_const( mDXRuby, "K_MEDIASELECT", INT2FIX(DIK_MEDIASELECT) );
    rb_define_const( mDXRuby, "K_BACKSPACE"  , INT2FIX(DIK_BACK) );
    rb_define_const( mDXRuby, "K_NUMPADSTAR" , INT2FIX(DIK_MULTIPLY) );
    rb_define_const( mDXRuby, "K_LALT"       , INT2FIX(DIK_LMENU) );
    rb_define_const( mDXRuby, "K_CAPSLOCK"   , INT2FIX(DIK_CAPITAL) );
    rb_define_const( mDXRuby, "K_NUMPADMINUS", INT2FIX(DIK_SUBTRACT) );
    rb_define_const( mDXRuby, "K_NUMPADPLUS" , INT2FIX(DIK_ADD) );
    rb_define_const( mDXRuby, "K_NUMPADPERIOD", INT2FIX(DIK_DECIMAL) );
    rb_define_const( mDXRuby, "K_NUMPADSLASH", INT2FIX(DIK_DIVIDE) );
    rb_define_const( mDXRuby, "K_RALT"       , INT2FIX(DIK_RMENU) );
    rb_define_const( mDXRuby, "K_UPARROW"    , INT2FIX(DIK_UP) );
    rb_define_const( mDXRuby, "K_PGUP"       , INT2FIX(DIK_PRIOR) );
    rb_define_const( mDXRuby, "K_LEFTARROW"  , INT2FIX(DIK_LEFT) );
    rb_define_const( mDXRuby, "K_RIGHTARROW" , INT2FIX(DIK_RIGHT) );
    rb_define_const( mDXRuby, "K_DOWNARROW"  , INT2FIX(DIK_DOWN) );
    rb_define_const( mDXRuby, "K_PGDN"       , INT2FIX(DIK_NEXT) );
    rb_define_const( mDXRuby, "K_CIRCUMFLEX" , INT2FIX(DIK_CIRCUMFLEX) );

    rb_define_const( mDXRuby, "P_UP"         , INT2FIX(P_UP)       );
    rb_define_const( mDXRuby, "P_LEFT"       , INT2FIX(P_LEFT)     );
    rb_define_const( mDXRuby, "P_RIGHT"      , INT2FIX(P_RIGHT)    );
    rb_define_const( mDXRuby, "P_DOWN"       , INT2FIX(P_DOWN)     );
    rb_define_const( mDXRuby, "P_BUTTON0"    , INT2FIX(P_BUTTON0)  );
    rb_define_const( mDXRuby, "P_BUTTON1"    , INT2FIX(P_BUTTON1)  );
    rb_define_const( mDXRuby, "P_BUTTON2"    , INT2FIX(P_BUTTON2)  );
    rb_define_const( mDXRuby, "P_BUTTON3"    , INT2FIX(P_BUTTON3)  );
    rb_define_const( mDXRuby, "P_BUTTON4"    , INT2FIX(P_BUTTON4)  );
    rb_define_const( mDXRuby, "P_BUTTON5"    , INT2FIX(P_BUTTON5)  );
    rb_define_const( mDXRuby, "P_BUTTON6"    , INT2FIX(P_BUTTON6)  );
    rb_define_const( mDXRuby, "P_BUTTON7"    , INT2FIX(P_BUTTON7)  );
    rb_define_const( mDXRuby, "P_BUTTON8"    , INT2FIX(P_BUTTON8)  );
    rb_define_const( mDXRuby, "P_BUTTON9"    , INT2FIX(P_BUTTON9)  );
    rb_define_const( mDXRuby, "P_BUTTON10"   , INT2FIX(P_BUTTON10) );
    rb_define_const( mDXRuby, "P_BUTTON11"   , INT2FIX(P_BUTTON11) );
    rb_define_const( mDXRuby, "P_BUTTON12"   , INT2FIX(P_BUTTON12) );
    rb_define_const( mDXRuby, "P_BUTTON13"   , INT2FIX(P_BUTTON13) );
    rb_define_const( mDXRuby, "P_BUTTON14"   , INT2FIX(P_BUTTON14) );
    rb_define_const( mDXRuby, "P_BUTTON15"   , INT2FIX(P_BUTTON15) );

    rb_define_const( mDXRuby, "P_D_UP"         , INT2FIX(P_D_UP)   );
    rb_define_const( mDXRuby, "P_D_LEFT"       , INT2FIX(P_D_LEFT) );
    rb_define_const( mDXRuby, "P_D_RIGHT"      , INT2FIX(P_D_RIGHT));
    rb_define_const( mDXRuby, "P_D_DOWN"       , INT2FIX(P_D_DOWN) );
    rb_define_const( mDXRuby, "P_L_UP"         , INT2FIX(P_L_UP)   );
    rb_define_const( mDXRuby, "P_L_LEFT"       , INT2FIX(P_L_LEFT) );
    rb_define_const( mDXRuby, "P_L_RIGHT"      , INT2FIX(P_L_RIGHT));
    rb_define_const( mDXRuby, "P_L_DOWN"       , INT2FIX(P_L_DOWN) );
    rb_define_const( mDXRuby, "P_R_UP"         , INT2FIX(P_R_UP)   );
    rb_define_const( mDXRuby, "P_R_LEFT"       , INT2FIX(P_R_LEFT) );
    rb_define_const( mDXRuby, "P_R_RIGHT"      , INT2FIX(P_R_RIGHT));
    rb_define_const( mDXRuby, "P_R_DOWN"       , INT2FIX(P_R_DOWN) );
 
    rb_define_const( mDXRuby, "M_LBUTTON"    , INT2FIX(M_LBUTTON)  );
    rb_define_const( mDXRuby, "M_RBUTTON"    , INT2FIX(M_RBUTTON)  );
    rb_define_const( mDXRuby, "M_MBUTTON"    , INT2FIX(M_MBUTTON)  );

    /* ���蓖�ď����� */
    for( i = 0; i < 256; i++)
    {
        g_diKeyConfig[i] = -1;
        g_diKeyCount[i] = 0; /* �J�E���g������ */
    }
    for( i = 0; i < PADMAX; i++)
    {
        for( j = 0; j < PADBUTTON_MAX; j++)
        {
            g_PadState[i].PadConfig[j] = -1;
            g_PadState[i].count[j] = 0; /* �J�E���g������ */
        }
    }

    /* �f�t�H���g�̊��蓖�� */
    Input_SetConfig( 0, P_LEFT   , DIK_LEFT  );
    Input_SetConfig( 0, P_RIGHT  , DIK_RIGHT );
    Input_SetConfig( 0, P_UP     , DIK_UP    );
    Input_SetConfig( 0, P_DOWN   , DIK_DOWN  );
    Input_SetConfig( 0, P_BUTTON0, DIK_Z     );
    Input_SetConfig( 0, P_BUTTON1, DIK_X     );
    Input_SetConfig( 0, P_BUTTON2, DIK_C     );

//    Input_SetConfig( 0, P_D_LEFT   , DIK_DELETE );
//    Input_SetConfig( 0, P_D_RIGHT  , DIK_NEXT   );
//    Input_SetConfig( 0, P_D_UP     , DIK_HOME   );
//    Input_SetConfig( 0, P_D_DOWN   , DIK_END    );

//    Input_SetConfig( 0, P_R_LEFT   , DIK_NUMPAD4 );
//    Input_SetConfig( 0, P_R_RIGHT  , DIK_NUMPAD6 );
//    Input_SetConfig( 0, P_R_UP     , DIK_NUMPAD8 );
//    Input_SetConfig( 0, P_R_DOWN   , DIK_NUMPAD2 );

    rb_define_const( mDXRuby, "IDC_ARROW"      , INT2FIX(32512) );
    rb_define_const( mDXRuby, "IDC_IBEAM"      , INT2FIX(32513) );
    rb_define_const( mDXRuby, "IDC_WAIT"       , INT2FIX(32514) );
    rb_define_const( mDXRuby, "IDC_CROSS"      , INT2FIX(32515) );
    rb_define_const( mDXRuby, "IDC_UPARROW"    , INT2FIX(32516) );
    rb_define_const( mDXRuby, "IDC_SIZE"       , INT2FIX(32640) );
    rb_define_const( mDXRuby, "IDC_ICON"       , INT2FIX(32641) );
    rb_define_const( mDXRuby, "IDC_SIZENWSE"   , INT2FIX(32642) );
    rb_define_const( mDXRuby, "IDC_SIZENESW"   , INT2FIX(32643) );
    rb_define_const( mDXRuby, "IDC_SIZEWE"     , INT2FIX(32644) );
    rb_define_const( mDXRuby, "IDC_SIZENS"     , INT2FIX(32645) );
    rb_define_const( mDXRuby, "IDC_SIZEALL"    , INT2FIX(32646) );
    rb_define_const( mDXRuby, "IDC_NO"         , INT2FIX(32648) );
    rb_define_const( mDXRuby, "IDC_HAND"       , INT2FIX(32649) );
    rb_define_const( mDXRuby, "IDC_APPSTARTING", INT2FIX(32650) );
    rb_define_const( mDXRuby, "IDC_HELP"       , INT2FIX(32651) );

#ifdef DXRUBY15
    /* Input::IME���W���[���o�^ */
    mIME = rb_define_module_under( mInput, "IME" );

    /* Input::IME���W���[���Ƀ��\�b�h�o�^ */
    rb_define_singleton_method( mIME, "get_string" , Input_IME_getstring , 0 );
    rb_define_singleton_method( mIME, "getString" ,  Input_IME_getstring , 0 );
    rb_define_singleton_method( mIME, "enabled?"  ,  Input_IME_getenable , 0 );
    rb_define_singleton_method( mIME, "enable="   ,  Input_IME_setenable , 1 );
    rb_define_singleton_method( mIME, "compositing?" ,  Input_IME_getcompositing , 0 );
    rb_define_singleton_method( mIME, "push_keys",  Input_IME_getpushkeys , 0 );
    rb_define_singleton_method( mIME, "pushKeys" ,  Input_IME_getpushkeys , 0 );
    rb_define_singleton_method( mIME, "release_keys",  Input_IME_getreleasekeys , 0 );
    rb_define_singleton_method( mIME, "releaseKeys" ,  Input_IME_getreleasekeys , 0 );
    rb_define_singleton_method( mIME, "get_comp_info" ,  Input_IME_getcompositioninfo , 0 );
    rb_define_singleton_method( mIME, "getCompInfo" ,  Input_IME_getcompositioninfo , 0 );

    rb_define_const( mIME, "ATTR_INPUT"       , INT2FIX(ATTR_INPUT) );
    rb_define_const( mIME, "ATTR_TARGET_CONVERTED"       , INT2FIX(ATTR_TARGET_CONVERTED) );
    rb_define_const( mIME, "ATTR_CONVERTED"       , INT2FIX(ATTR_CONVERTED) );
    rb_define_const( mIME, "ATTR_TARGET_NOTCONVERTED"       , INT2FIX(ATTR_TARGET_NOTCONVERTED) );
    rb_define_const( mIME, "ATTR_INPUT_ERROR"       , INT2FIX(ATTR_INPUT_ERROR) );

    cCompInfo = rb_struct_define( NULL, "comp_str", "comp_attr", "cursor_pos", "can_list", "selection", "selection_total", "page_size", "total", 0 );
    rb_define_const( mIME, "CompInfo", cCompInfo );

    {
        VALUE ary[8];
        ary[0] = rb_str_new2( "" );
        ary[1] = rb_ary_new();
        ary[2] = INT2FIX( 0 );
        ary[3] = rb_ary_new();
        ary[4] = Qnil;
        ary[5] = Qnil;
        ary[6] = Qnil;
        ary[7] = Qnil;
        g_vCompInfoArray = rb_ary_new4( 8, ary );
    }
    rb_global_variable( &g_vCompInfoArray );
#endif

    SetImeEnable( FALSE );
}


