#define WINVER 0x0500                                  /* �o�[�W������` Windows2000�ȏ� */
#define DIRECTSOUND_VERSION 0x0900
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#include <dmusici.h>

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "sound.h"

#ifndef DS3DALG_DEFAULT
GUID DS3DALG_DEFAULT = {0};
#endif

#define WAVE_RECT 0
#define WAVE_SIN 1
#define WAVE_SAW 2
#define WAVE_TRI 3

static VALUE cSound;        /* �T�E���h�N���X       */
static VALUE cSoundEffect;  /* �������ʉ��N���X     */

static IDirectMusicPerformance8 *g_pDMPerformance = NULL;       /* DirectMusicPerformance8�C���^�[�t�F�C�X */
static IDirectMusicLoader8      *g_pDMLoader = NULL;            /* ���[�_�[ */
static LPDIRECTSOUND8           g_pDSound = NULL;               /* DirectSound�C���^�[�t�F�C�X */
static int g_iRefDM = 0; /* DirectMusic�p�t�H�[�}���X�̎Q�ƃJ�E���g */
static int g_iRefDS = 0; /* DirectSound�̎Q�ƃJ�E���g */

/* Sound�I�u�W�F�N�g */
struct DXRubySound {
    IDirectMusicAudioPath8   *pDMDefAudioPath; /* �f�t�H���g�I�[�f�B�I�p�X */
    IDirectMusicSegment8     *pDMSegment;        /* �Z�O�����g       */
    int start;
    int loopstart;
    int loopend;
    int loopcount;
    int midwavflag; /* mid��0�Awav��1 */
    VALUE vbuffer;
};

/* SoundEffect�I�u�W�F�N�g */
struct DXRubySoundEffect {
    LPDIRECTSOUNDBUFFER pDSBuffer;    /* �o�b�t�@         */
};



/*********************************************************************
 * Sound�N���X
 *
 * DirectMusic���g�p���ĉ���炷�B
 * �Ƃ肠���������o�����Ɗ撣���Ă���B
 *********************************************************************/

/*--------------------------------------------------------------------
   �Q�Ƃ���Ȃ��Ȃ����Ƃ���GC����Ă΂��֐�
 ---------------------------------------------------------------------*/
static void Sound_free( struct DXRubySound *sound )
{
    HRESULT hr;

    /* �T�E���h�I�u�W�F�N�g�̊J�� */
    /* �o���h��� */
    if ( sound->pDMSegment )
    {
        hr = sound->pDMSegment->lpVtbl->Unload( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Band release failed - Unload" );
        }
        /* �Z�O�����g���J�� */
        RELEASE( sound->pDMSegment );
    }

    /* �f�t�H���g�I�[�f�B�I�p�X���J�� */
    RELEASE( sound->pDMDefAudioPath );

    g_iRefDM--;

    if( g_iRefDM <= 0 )
    {
        /* ���t��~ */
        if ( g_pDMPerformance )
        {
            hr = g_pDMPerformance->lpVtbl->Stop( g_pDMPerformance, NULL, NULL, 0, 0 );
            if ( FAILED( hr ) )
            {
                rb_raise( eDXRubyError, "Stop performance failed - Stop" );
            }
            g_pDMPerformance->lpVtbl->CloseDown( g_pDMPerformance );
        }
        RELEASE(g_pDMPerformance);

        /* ���[�_���J�� */
        RELEASE(g_pDMLoader);
    }
}

static void Sound_mark( struct DXRubySound *sound )
{
    rb_gc_mark( sound->vbuffer );
}

static void Sound_release( struct DXRubySound *sound )
{
    if ( sound->pDMSegment )
    {
        Sound_free( sound );
    }
    free( sound );
    sound = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Sound_data_type = {
    "Sound",
    {
    Sound_mark,
    Sound_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Sound�N���X��dispose�B
 ---------------------------------------------------------------------*/
static VALUE Sound_dispose( VALUE self )
{
    struct DXRubySound *sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    Sound_free( sound );
    return self;
}

/*--------------------------------------------------------------------
   Sound�N���X��disposed?�B
 ---------------------------------------------------------------------*/
static VALUE Sound_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( Sound, self )->pDMSegment == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   Sound�N���X��allocate�B���������m�ۂ���ׂ�initialize�O�ɌĂ΂��B
 ---------------------------------------------------------------------*/
static VALUE Sound_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubySound *sound;

    /* DXRubyImage�̃������擾��Image�I�u�W�F�N�g���� */
    sound = malloc(sizeof(struct DXRubySound));
    if( sound == NULL ) rb_raise( eDXRubyError, "Out of memory - Sound_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Sound_data_type, sound );
#else
    obj = Data_Wrap_Struct(klass, 0, Sound_release, sound);
#endif
    /* �Ƃ肠�����T�E���h�I�u�W�F�N�g��NULL�ɂ��Ă��� */
    sound->pDMSegment = NULL;
    sound->vbuffer = Qnil;

    return obj;
}


/*--------------------------------------------------------------------
   Sound�N���X��load_from_memory�B�t�@�C�������������烍�[�h����B
 ---------------------------------------------------------------------*/
static VALUE Sound_load_from_memory( VALUE klass, VALUE vstr, VALUE vtype )
{
    HRESULT hr;
    WCHAR wstrFileName[MAX_PATH];
    VALUE obj;
    struct DXRubySound *sound;
    CHAR strPath[MAX_PATH];
    DWORD i;
    WCHAR wstrSearchPath[MAX_PATH];
    VALUE vsjisstr;

    g_iRefAll++;

    Check_Type( vstr, T_STRING );

    if( g_iRefDM == 0 )
    {
        /* �p�t�H�[�}���X�̍쐬 */
        hr = CoCreateInstance( &CLSID_DirectMusicPerformance, NULL,
                               CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8,
                               (void**)&g_pDMPerformance );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
        }

        /* �p�t�H�[�}���X�̏����� */
        hr = g_pDMPerformance->lpVtbl->InitAudio( g_pDMPerformance,
                                                  NULL,                  /* IDirectMusic�C���^�[�t�F�C�X�͕s�v */
                                                  NULL,                  /* IDirectSound�C���^�[�t�F�C�X�͕s�v */
                                                  g_hWnd,               /* �E�B���h�E�̃n���h�� */
                                                  DMUS_APATH_SHARED_STEREOPLUSREVERB,  /* �f�t�H���g�̃I�[�f�B�I�p�X�E�^�C�v */
                                                  64,                    /* �p�t�H�[�}���X�E�`�����l���̐� */
                                                  DMUS_AUDIOF_ALL,       /* �V���Z�T�C�U�̋@�\ */
                                                  NULL );                /* �I�[�f�B�I�E�p�����[�^�ɂ̓f�t�H���g���g�p */
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - InitAudio" );
        }

        /* ���[�_�[�̍쐬 */
        hr = CoCreateInstance( &CLSID_DirectMusicLoader, NULL, 
                               CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8,
                               (void**)&g_pDMLoader );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
        }

        /* ���[�_�[�̏������i�����p�X���J�����g�E�f�B���N�g���ɐݒ�j */
        i = GetCurrentDirectory( MAX_PATH, strPath );
        if ( i == 0 || MAX_PATH < i )
        {
            rb_raise( eDXRubyError, "Get current directory failed - GetCurrentDirectory" );
        }

        /* �}���`�E�o�C�g������UNICODE�ɕϊ� */
        MultiByteToWideChar( CP_ACP, 0, strPath, -1, wstrSearchPath, MAX_PATH );

        /* ���[�_�[�Ɍ����p�X��ݒ� */
        hr = g_pDMLoader->lpVtbl->SetSearchDirectory( g_pDMLoader, &GUID_DirectMusicAllTypes,
                                                      wstrSearchPath, FALSE );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set directory failed - SetSearchDirectory" );
        }
    }
    g_iRefDM++;

    /* �T�E���h�I�u�W�F�N�g�擾 */
    obj = Sound_allocate( klass );
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    if( sound->pDMSegment )
    {
        g_iRefDM++;
        Sound_free( sound );
        g_iRefDM--;
        g_iRefAll--;
    }

    /* �I�[�f�B�I�E�p�X�쐬 */
    hr = g_pDMPerformance->lpVtbl->CreateStandardAudioPath( g_pDMPerformance,
        DMUS_APATH_DYNAMIC_STEREO,      /* �p�X�̎�ށB */
        64,                             /* �p�t�H�[�}���X �`�����l���̐��B */
        TRUE,                           /* �����ŃA�N�e�B�u�ɂȂ�B */
        &sound->pDMDefAudioPath );      /* �I�[�f�B�I�p�X���󂯎��|�C���^�B */

    if ( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "AudioPath set error - CreateStandardAudioPath" );
    }

    sound->vbuffer = vstr;

    {
        DMUS_OBJECTDESC desc;

        ZeroMemory( &desc, sizeof(DMUS_OBJECTDESC) );
        desc.dwSize      = sizeof(DMUS_OBJECTDESC);
        desc.dwValidData = DMUS_OBJ_MEMORY | DMUS_OBJ_CLASS;
        desc.guidClass   = CLSID_DirectMusicSegment;
        desc.llMemLength = (LONGLONG)RSTRING_LEN(vstr);      // �o�b�t�@�̃T�C�Y
        desc.pbMemData   = (LPBYTE)RSTRING_PTR(vstr);        // �f�[�^�̓����Ă���o�b�t�@

        hr = g_pDMLoader->lpVtbl->GetObject( g_pDMLoader, &desc, &IID_IDirectMusicSegment8, (void**)&sound->pDMSegment );
    }

    if( FAILED( hr ) )
    {
        sound->pDMSegment = NULL;
        rb_raise( eDXRubyError, "Load error - LoadObjectFromFile" );
    }

    sound->start = 0;
    sound->loopstart = 0;
    sound->loopend = 0;

    /* MIDI�̏ꍇ */
    if( NUM2INT( vtype ) == 0 )
    {
        hr = sound->pDMSegment->lpVtbl->SetParam( sound->pDMSegment, &GUID_StandardMIDIFile,
                                                  0xFFFFFFFF, 0, 0, NULL);
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Load error - SetParam" );
        }
        sound->loopcount = DMUS_SEG_REPEAT_INFINITE;
        sound->midwavflag = 0;
        /* ���[�v�񐔐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop count failed - SetRepeats" );
        }
    }
    else
    {
        sound->loopcount = 1;
        sound->midwavflag = 1;
    }

    /* �o���h�_�E�����[�h */
    hr = sound->pDMSegment->lpVtbl->Download( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Band loading failed - Download" );
    }


    /* ���ʐݒ� */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, 230 * 9600 / 255 - 9600 , 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume failed - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   Sound�N���X��initialize�B�t�@�C�������[�h����B
 ---------------------------------------------------------------------*/
static VALUE Sound_initialize( VALUE obj, VALUE vfilename )
{
    HRESULT hr;
    WCHAR wstrFileName[MAX_PATH];
    struct DXRubySound *sound;
    CHAR strPath[MAX_PATH];
    DWORD i;
    WCHAR wstrSearchPath[MAX_PATH];
    VALUE vsjisstr;

    g_iRefAll++;

	Check_Type(vfilename, T_STRING);

    if( g_iRefDM == 0 )
	{
	    /* �p�t�H�[�}���X�̍쐬 */
	    hr = CoCreateInstance( &CLSID_DirectMusicPerformance, NULL,
	                           CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8,
	                           (void**)&g_pDMPerformance );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
	    }

	    /* �p�t�H�[�}���X�̏����� */
	    hr = g_pDMPerformance->lpVtbl->InitAudio( g_pDMPerformance,
	                                              NULL,                  /* IDirectMusic�C���^�[�t�F�C�X�͕s�v */
	                                              NULL,                  /* IDirectSound�C���^�[�t�F�C�X�͕s�v */
	                                              g_hWnd,               /* �E�B���h�E�̃n���h�� */
	                                              DMUS_APATH_SHARED_STEREOPLUSREVERB,  /* �f�t�H���g�̃I�[�f�B�I�p�X�E�^�C�v */
	                                              64,                    /* �p�t�H�[�}���X�E�`�����l���̐� */
	                                              DMUS_AUDIOF_ALL,       /* �V���Z�T�C�U�̋@�\ */
	                                              NULL );                /* �I�[�f�B�I�E�p�����[�^�ɂ̓f�t�H���g���g�p */
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - InitAudio" );
	    }

	    /* ���[�_�[�̍쐬 */
	    hr = CoCreateInstance( &CLSID_DirectMusicLoader, NULL, 
	                           CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8,
	                           (void**)&g_pDMLoader );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
	    }

	    /* ���[�_�[�̏������i�����p�X���J�����g�E�f�B���N�g���ɐݒ�j */
	    i = GetCurrentDirectory( MAX_PATH, strPath );
	    if ( i == 0 || MAX_PATH < i )
	    {
	        rb_raise( eDXRubyError, "Get current directory failed - GetCurrentDirectory" );
	    }

	    /* �}���`�E�o�C�g������UNICODE�ɕϊ� */
	    MultiByteToWideChar( CP_ACP, 0, strPath, -1, wstrSearchPath, MAX_PATH );

	    /* ���[�_�[�Ɍ����p�X��ݒ� */
	    hr = g_pDMLoader->lpVtbl->SetSearchDirectory( g_pDMLoader, &GUID_DirectMusicAllTypes,
	                                                  wstrSearchPath, FALSE );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "Set directory failed - SetSearchDirectory" );
	    }
	}
    g_iRefDM++;

	/* �T�E���h�I�u�W�F�N�g�擾 */
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    if( sound->pDMSegment )
    {
        g_iRefDM++;
        Sound_free( sound );
        g_iRefDM--;
        g_iRefAll--;
    }

	/* �I�[�f�B�I�E�p�X�쐬 */
	hr = g_pDMPerformance->lpVtbl->CreateStandardAudioPath( g_pDMPerformance,
		DMUS_APATH_DYNAMIC_STEREO,      /* �p�X�̎�ށB */
		64,                             /* �p�t�H�[�}���X �`�����l���̐��B */
		TRUE,                           /* �����ŃA�N�e�B�u�ɂȂ�B */
		&sound->pDMDefAudioPath );      /* �I�[�f�B�I�p�X���󂯎��|�C���^�B */

	if ( FAILED( hr ) )
	{
        rb_raise( eDXRubyError, "AudioPath set error - CreateStandardAudioPath" );
	}

    /* �t�@�C�����[�h */
    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, g_enc_sys );
    }
    else
    {
        vsjisstr = vfilename;
    }

    MultiByteToWideChar( CP_ACP, 0, RSTRING_PTR( vsjisstr ), -1, wstrFileName, MAX_PATH );
    hr = g_pDMLoader->lpVtbl->LoadObjectFromFile( g_pDMLoader, &CLSID_DirectMusicSegment,
                                                  &IID_IDirectMusicSegment8,
                                                  wstrFileName,
                                                  (LPVOID*)&sound->pDMSegment );
    if( FAILED( hr ) )
    {
        sound->pDMSegment = NULL;
        rb_raise( eDXRubyError, "Load error - LoadObjectFromFile" );
    }

    sound->start = 0;
    sound->loopstart = 0;
    sound->loopend = 0;

    /* MIDI�̏ꍇ */
    if( strstr( RSTRING_PTR( vsjisstr ), ".mid" ) != NULL )
    {
        hr = sound->pDMSegment->lpVtbl->SetParam( sound->pDMSegment, &GUID_StandardMIDIFile,
                                                  0xFFFFFFFF, 0, 0, NULL);
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Load error - SetParam" );
        }
        sound->loopcount = DMUS_SEG_REPEAT_INFINITE;
        sound->midwavflag = 0;
        /* ���[�v�񐔐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop count failed - SetRepeats" );
        }
    }
    else
    {
        sound->loopcount = 1;
        sound->midwavflag = 1;
    }

    /* �o���h�_�E�����[�h */
    hr = sound->pDMSegment->lpVtbl->Download( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Band loading failed - Download" );
    }


    /* ���ʐݒ� */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, 230 * 9600 / 255 - 9600 , 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume failed - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   �J�n�ʒu��ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setStart( VALUE obj, VALUE vstart )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->start = NUM2INT( vstart );

    if( sound->midwavflag == 1 && sound->start > 0 )    /* wav�̏ꍇ */
    {
        hr = sound->pDMSegment->lpVtbl->SetLength( sound->pDMSegment, sound->start * DMUS_PPQ / 768 + 1 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set start point failed - SetLength" );
        }
    }

    /* �Z�O�����g�Đ��X�^�[�g�ʒu�ݒ� */
    hr = sound->pDMSegment->lpVtbl->SetStartPoint( sound->pDMSegment, sound->start * DMUS_PPQ / 768 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set start point failed - SetStartPoint" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   ���[�v�J�n�ʒu��ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopStart( VALUE obj, VALUE vloopstart )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopstart = NUM2INT( vloopstart );

    if( sound->midwavflag == 1 )
    {
        rb_raise( eDXRubyError, "Can not be set to Wav data - Sound_loopStart=" );
    }

    if( sound->loopstart <= sound->loopend )
    {
        /* ���[�v�͈͐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, sound->loopstart * DMUS_PPQ / 768
                                                                        , sound->loopend * DMUS_PPQ / 768 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }
    else
    {
        /* ���[�v�͈͐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, 0, 0 );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }

    return obj;
}


/*--------------------------------------------------------------------
   ���[�v�I���ʒu��ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopEnd( VALUE obj, VALUE vloopend )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopend = NUM2INT( vloopend );

    if( sound->midwavflag == 1 )
    {
        rb_raise( eDXRubyError, "Can not be set to Wav data - Sound_loopEnd=" );
    }

    if( sound->loopstart <= sound->loopend )
    {
        /* ���[�v�͈͐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, sound->loopstart * DMUS_PPQ / 768
                                                                        , sound->loopend * DMUS_PPQ / 768 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }
    else
    {
        /* ���[�v�͈͐ݒ� */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, 0, 0 );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }


    return obj;
}


/*--------------------------------------------------------------------
   ���[�v�񐔂�ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopCount( VALUE obj, VALUE vloopcount )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopcount = NUM2INT( vloopcount );

    /* ���[�v�񐔐ݒ� */
    hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failed to set loop count - SetRepeats" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   ���ʂ�ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setVolume( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;
    VALUE vvolume, vtime;
    int volume, time;

    rb_scan_args( argc, argv, "11", &vvolume, &vtime );

    time = vtime == Qnil ? 0 : NUM2INT( vtime );
    volume = NUM2INT( vvolume ) > 255 ? 255 : NUM2INT( vvolume );
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* ���ʐݒ� */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, volume * 9600 / 255 - 9600, time );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume error - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   �p�����擾����
 ---------------------------------------------------------------------*/
static VALUE Sound_getPan( VALUE self )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;
    long result;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - GetPan" );
    }

    /* �p���擾 */
    hr = pDS3DBuffer->lpVtbl->GetPan( pDS3DBuffer, &result );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "pan get error - GetPan" );
    }

    RELEASE( pDS3DBuffer );

    return INT2NUM( result );
}


/*--------------------------------------------------------------------
   �p����ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setPan( VALUE self, VALUE vpan )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - SetPan" );
    }

    /* �p���ݒ� */
    hr = pDS3DBuffer->lpVtbl->SetPan( pDS3DBuffer, NUM2INT( vpan ) );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "pan setting error - SetPan" );
    }

    RELEASE( pDS3DBuffer );

    return self;
}


/*--------------------------------------------------------------------
   ���g�����擾����
 ---------------------------------------------------------------------*/
static VALUE Sound_getFrequency( VALUE self )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;
    DWORD result;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - GetPan" );
    }

    /* ���g���擾 */
    hr = pDS3DBuffer->lpVtbl->GetFrequency( pDS3DBuffer, &result );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "frequency get error - getFrequency" );
    }

    RELEASE( pDS3DBuffer );

    return UINT2NUM( result );
}


/*--------------------------------------------------------------------
   ���g����ݒ肷��
 ---------------------------------------------------------------------*/
static VALUE Sound_setFrequency( VALUE self, VALUE vfrequency )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - SetPan" );
    }

    /* ���g���ݒ� */
    hr = pDS3DBuffer->lpVtbl->SetFrequency( pDS3DBuffer, NUM2UINT( vfrequency ) );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "frequency setting error - setFrequency" );
    }

    RELEASE( pDS3DBuffer );

    return self;
}


/*--------------------------------------------------------------------
   ����炷
 ---------------------------------------------------------------------*/
static VALUE Sound_play( VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* �Đ� */
    if( sound->midwavflag == 0 )
    {
        hr = g_pDMPerformance->lpVtbl->PlaySegmentEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, NULL, NULL,
                                                      DMUS_SEGF_CONTROL, 0, NULL, NULL, (IUnknown* )sound->pDMDefAudioPath );
    }
    else
    {
        hr = g_pDMPerformance->lpVtbl->PlaySegmentEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, NULL, NULL,
                                                      DMUS_SEGF_SECONDARY, 0, NULL, NULL, (IUnknown* )sound->pDMDefAudioPath );
    }
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - PlaySegmentEx" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   �����~�߂�
 ---------------------------------------------------------------------*/
static VALUE Sound_stop( VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* �Đ� */
    hr = g_pDMPerformance->lpVtbl->StopEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, 0, 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound stop failed - StopEx" );
    }

    return obj;

}


/*********************************************************************
 * SoundEffect�N���X
 *
 * DirectSound���g�p���ĉ���炷�B
 * �Ƃ肠���������o�����Ɗ撣���Ă���B
 *********************************************************************/

/*--------------------------------------------------------------------
   �Q�Ƃ���Ȃ��Ȃ����Ƃ���GC����Ă΂��֐�
 ---------------------------------------------------------------------*/
static void SoundEffect_free( struct DXRubySoundEffect *soundeffect )
{
    /* �T�E���h�o�b�t�@���J�� */
    RELEASE( soundeffect->pDSBuffer );

    g_iRefDS--;

    if( g_iRefDS == 0 )
    {
        RELEASE( g_pDSound );
    }

}

static void SoundEffect_release( struct DXRubySoundEffect *soundeffect )
{
    if( soundeffect->pDSBuffer )
    {
        SoundEffect_free( soundeffect );
    }
    free( soundeffect );
    soundeffect = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t SoundEffect_data_type = {
    "SoundEffect",
    {
    0,
    SoundEffect_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   SoundEffect�N���X��dispose�B
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_dispose( VALUE self )
{
    struct DXRubySoundEffect *soundeffect = DXRUBY_GET_STRUCT( SoundEffect, self );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );
    SoundEffect_free( soundeffect );
    return self;
}

/*--------------------------------------------------------------------
   SoundEffect�N���X��disposed?�B
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( SoundEffect, self )->pDSBuffer == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   SoundEffect�N���X��allocate�B���������m�ۂ���ׂ�initialize�O�ɌĂ΂��B
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubySoundEffect *soundeffect;

    /* DXRubySoundEffect�̃������擾��SoundEffect�I�u�W�F�N�g���� */
    soundeffect = malloc(sizeof(struct DXRubySoundEffect));
    if( soundeffect == NULL ) rb_raise( eDXRubyError, "Out of memory - SoundEffect_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &SoundEffect_data_type, soundeffect );
#else
    obj = Data_Wrap_Struct(klass, 0, SoundEffect_release, soundeffect);
#endif

    /* �Ƃ肠�����T�E���h�I�u�W�F�N�g��NULL�ɂ��Ă��� */
    soundeffect->pDSBuffer = NULL;

    return obj;
}


static short calcwave(int type, double vol, double count, double p, double duty)
{
	switch( type )
	{
	case 1: /* �T�C���g */
		return (short)((sin( (3.141592653589793115997963468544185161590576171875f * 2) * (double)count / (double)p )) * (double)vol * 128);
		break;
	case 2: /* �m�R�M���g */
		return (short)(((double)count / (double)p - 0.5) * (double)vol * 256);
		break;
	case 3: /* �O�p�g */
		if( count < p / 4 )			/* 1/4 */
		{
			return (short)((double)count / ((double)p / 4) * (double)vol * 128);
		}
		else if( count < p / 2 )		/* 2/4 */
		{
			return (short)(((double)p / 2 - (double)count) / ((double)p / 4) * (double)vol * 128);
		}
		else if( count < p * 3 / 4 )	/* 3/4 */
		{
			return (short)(-((double)count - (double)p / 2)/ ((double)p / 4) * (double)vol * 128);
		}
		else											/* �Ō� */
		{
			return (short)(-((double)p - (double)count) / ((double)p / 4) * (double)vol * 128);
		}
		break;
	case 0: /* ��`�g */
	default: /* �f�t�H���g */
		if( count < p * duty )	/* �O�� */
		{
			return (short)(vol * 128);
		}
		else									/* �㔼 */
		{
		    return (short)(-vol * 128);
		}
		break;
	}
}


/*--------------------------------------------------------------------
   SoundEffect�N���X��initialize�B�g�`�𐶐�����B
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_initialize( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;
    DSBUFFERDESC desc;
    WAVEFORMATEX pcmwf;
    int i;
    short *pointer, *pointer2;
    DWORD size, size2;
    VALUE vf;
    double count, duty = 0.5;
    double vol;
    double f;
    VALUE vsize, vtype, vresolution;
    int type, resolution;

    g_iRefAll++;

    rb_scan_args( argc, argv, "12", &vsize, &vtype, &vresolution );

    type       = vtype       == Qnil ? 0    : NUM2INT( vtype );
    resolution = vresolution == Qnil ? 1000 : (NUM2INT( vresolution ) > 44100 ? 44100 : NUM2INT( vresolution ));

    /* DirectSound�I�u�W�F�N�g�̍쐬 */
    if( g_iRefDS == 0 )
    {
        hr = DirectSoundCreate8( &DSDEVID_DefaultPlayback, &g_pDSound, NULL );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectSound initialize error - DirectSoundCreate8" );
        }

        hr = g_pDSound->lpVtbl->SetCooperativeLevel( g_pDSound, g_hWnd, DSSCL_PRIORITY );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectSound initialize error - SetCooperativeLevel" );
        }
    }
    g_iRefDS++;

    /* �T�E���h�I�u�W�F�N�g�쐬 */
    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    if( soundeffect->pDSBuffer )
    {
        g_iRefDS++;
        SoundEffect_free( soundeffect );
        g_iRefDS--;
        g_iRefAll++;
    }

    /* �T�E���h�o�b�t�@�쐬 */
    pcmwf.wFormatTag = WAVE_FORMAT_PCM;
    pcmwf.nChannels = 1;
    pcmwf.nSamplesPerSec = 44100;
    pcmwf.wBitsPerSample = 16;
    pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
    pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
    pcmwf.cbSize = 0;

    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS;
#ifdef DXRUBY15
    if( TYPE( vsize ) == T_ARRAY )
    {
        desc.dwBufferBytes = RARRAY_LEN( vsize ) * (pcmwf.nChannels * pcmwf.wBitsPerSample / 8);
    }
    else
    {
#endif
        desc.dwBufferBytes = pcmwf.nAvgBytesPerSec / 100 * NUM2INT(vsize) / 10;
#ifdef DXRUBY15
    }
#endif
    desc.dwReserved = 0;
    desc.lpwfxFormat = &pcmwf;
    desc.guid3DAlgorithm = DS3DALG_DEFAULT;

    hr = g_pDSound->lpVtbl->CreateSoundBuffer( g_pDSound, &desc, &soundeffect->pDSBuffer, NULL );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to create the SoundBuffer - CreateSoundBuffer" );
    }

    /* ���b�N */
    hr = soundeffect->pDSBuffer->lpVtbl->Lock( soundeffect->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    if( TYPE( vsize ) == T_ARRAY )
    {
        /* ���o�b�t�@���� */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            double tmp = NUM2DBL( RARRAY_AREF( vsize, i ) );
            if( tmp < 0.0 )
            {
                if( tmp < -1.0 ) tmp = -1.0;
                pointer[i] = (short)(tmp * 32768.0);
            }
            else
            {
                if( tmp > 1.0 ) tmp = 1.0;
                pointer[i] = (short)(tmp * 32767.0);
            }
        }
    }
    else
    {
        /* ���o�b�t�@������ */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            pointer[i] = 0;
        }

        count = 0;

        /* �g�`���� */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            /* �w�莞�ԒP�ʂŃu���b�N���Ăяo�� */
            if ( i % (pcmwf.nSamplesPerSec / resolution) == 0 )
            {
                vf = rb_yield( obj );
                if( TYPE( vf ) != T_ARRAY )
                {
                    soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
                    rb_raise(rb_eTypeError, "invalid value - SoundEffect_initialize");
                    break;
                }
                f = NUM2DBL( rb_ary_entry(vf, 0) );
                vol = NUM2DBL(rb_ary_entry(vf, 1));
                if( RARRAY_LEN( vf ) > 2 )
                {
                    duty = NUM2DBL(rb_ary_entry(vf, 2));
                }
                /* �ő�/�Œ���g���ƍő�{�����[���̐��� */
                f = f > pcmwf.nSamplesPerSec / 2.0f ? pcmwf.nSamplesPerSec / 2.0f : f;
                f = f < 20 ? 20 : f;
                vol = vol > 255 ? 255 : vol;
                vol = vol < 0 ? 0 : vol;
            }
            count = count + f;
            if( count >= pcmwf.nSamplesPerSec )
            {
                count = count - pcmwf.nSamplesPerSec;
            }
            pointer[i] = calcwave(type, vol, count, pcmwf.nSamplesPerSec, duty);
        }
    }

    /* �A�����b�N */
    hr = soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return obj;

}


/*--------------------------------------------------------------------
   �g�`����������B
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_add( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;
	DSBUFFERDESC desc;
	WAVEFORMATEX pcmwf;
	int i;
	short *pointer, *pointer2;
	DWORD size, size2;
	VALUE vf, vtype, vresolution;
	double count, duty = 0.5;
	double vol;
    double f;
	int type, resolution;
	int data;

	rb_scan_args( argc, argv, "02", &vtype, &vresolution );

	type       = vtype       == Qnil ? 0    : NUM2INT( vtype );
	resolution = vresolution == Qnil ? 1000 : (NUM2INT( vresolution ) > 44100 ? 44100 : NUM2INT( vresolution ));

    /* �T�E���h�I�u�W�F�N�g�擾 */
    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );

	/* ���b�N */
	hr = soundeffect->pDSBuffer->lpVtbl->Lock( soundeffect->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

	pcmwf.nChannels = 1;
	pcmwf.nSamplesPerSec = 44100;
	pcmwf.wBitsPerSample = 16;
	pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
	pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	desc.dwBufferBytes = size;

	count = 0;

	/* �g�`���� */
	for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
	{
		/* �w�莞�ԒP�ʂŃu���b�N���Ăяo�� */
		if ( i % (pcmwf.nSamplesPerSec / resolution) == 0 )
		{
			vf = rb_yield( obj );
			if( TYPE( vf ) != T_ARRAY )
			{
				soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
			    rb_raise(rb_eTypeError, "invalid value - SoundEffect_add");
				break;
			}
			f = NUM2DBL( rb_ary_entry(vf, 0) );
			vol = NUM2DBL(rb_ary_entry(vf, 1));
            if( RARRAY_LEN( vf ) > 2 )
            {
                duty = NUM2DBL(rb_ary_entry(vf, 2));
            }
			/* �ő�/�Œ���g���ƍő�{�����[���̐��� */
			f = f > pcmwf.nSamplesPerSec / 2.0f ? pcmwf.nSamplesPerSec / 2.0f : f;
			f = f < 20 ? 20 : f;
			vol = vol > 255 ? 255 : vol;
			vol = vol < 0 ? 0 : vol;
		}
		count = count + f;
		if( count >= pcmwf.nSamplesPerSec )
		{
			count = count - pcmwf.nSamplesPerSec;
		}

		data = calcwave(type, vol, count, pcmwf.nSamplesPerSec, duty);

		if( data + (int)pointer[i] > 32767 )
		{
			pointer[i] = 32767;
		}
		else if( data + (int)pointer[i] < -32768 )
		{
			pointer[i] = -32768;
		}
		else
		{
			pointer[i] += data;
		}
	}

	/* �A�����b�N */
	hr = soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return obj;

}


/*--------------------------------------------------------------------
   ����炷
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_play( int argc, VALUE *argv, VALUE self )
{
    HRESULT hr;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );
    VALUE vflag;
    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    rb_scan_args( argc, argv, "01", &vflag );

    /* �Ƃ߂� */
    hr = se->pDSBuffer->lpVtbl->Stop( se->pDSBuffer );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }

    hr = se->pDSBuffer->lpVtbl->SetCurrentPosition( se->pDSBuffer, 0 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }

    /* �Đ� */
    hr = se->pDSBuffer->lpVtbl->Play( se->pDSBuffer, 0, 0, RTEST(vflag) ? DSBPLAY_LOOPING : 0 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }
    return self;
}


/*--------------------------------------------------------------------
   �����~�߂�
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_stop( VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;

    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );

    /* �Ƃ߂� */
    hr = soundeffect->pDSBuffer->lpVtbl->Stop( soundeffect->pDSBuffer );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "���̒�~�Ɏ��s���܂��� - Stop" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   wav�t�@�C���o��
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_save( VALUE self, VALUE vfilename )
{
    HRESULT hr;
    HANDLE hfile;
    short *pointer, *pointer2;
    DWORD size, size2;
    DWORD tmpl;
    WORD tmps;
    DWORD wsize;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );

    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    /* ���b�N */
    hr = se->pDSBuffer->lpVtbl->Lock( se->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    hfile = CreateFile( RSTRING_PTR( vfilename ), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if( hfile == INVALID_HANDLE_VALUE )
    {
        rb_raise( eDXRubyError, "Write failure - open" );
    }

    if( !WriteFile( hfile, "RIFF", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = size + 44 - 8;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, "WAVE", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, "fmt ", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 16;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 1;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 1;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 44100;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 44100*2;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 2;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 16;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );

    if( !WriteFile( hfile, "data", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = size;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, pointer, size, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );

    CloseHandle( hfile );

    /* �A�����b�N */
    hr = se->pDSBuffer->lpVtbl->Unlock( se->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return self;
}


/*--------------------------------------------------------------------
   �z��
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_to_a( VALUE self )
{
    HRESULT hr;
    short *pointer, *pointer2;
    DWORD size, size2;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );
    VALUE ary;
    int i;

    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    /* ���b�N */
    hr = se->pDSBuffer->lpVtbl->Lock( se->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    ary = rb_ary_new2( size / 2 );
    for( i = 0; i < size / 2; i++ )
    {
        double tmp;
        if( pointer[i] < 0 )
        {
            tmp = pointer[i] / 32768.0;
        }
        else
        {
            tmp = pointer[i] / 32767.0;
        }
        rb_ary_push( ary, rb_float_new( tmp ) );
    }

    /* �A�����b�N */
    hr = se->pDSBuffer->lpVtbl->Unlock( se->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return ary;
}


void Init_dxruby_Sound( void )
{
    /* Sound�N���X��` */
    cSound = rb_define_class_under( mDXRuby, "Sound", rb_cObject );
    rb_define_singleton_method( cSound, "load_from_memory", Sound_load_from_memory, 2 );
    rb_define_singleton_method( cSound, "loadFromMemory", Sound_load_from_memory, 2 );

    /* Sound�N���X�Ƀ��\�b�h�o�^*/
    rb_define_private_method( cSound, "initialize"   , Sound_initialize   , 1 );
    rb_define_method( cSound, "dispose"      , Sound_dispose   , 0 );
    rb_define_method( cSound, "disposed?"    , Sound_check_disposed, 0 );
    rb_define_method( cSound, "play"         , Sound_play      , 0 );
    rb_define_method( cSound, "stop"         , Sound_stop         , 0 );
    rb_define_method( cSound, "set_volume"   , Sound_setVolume    , -1 );
    rb_define_method( cSound, "setVolume"    , Sound_setVolume    , -1 );
    rb_define_method( cSound, "pan"          , Sound_getPan       , 0 );
    rb_define_method( cSound, "pan="         , Sound_setPan       , 1 );
    rb_define_method( cSound, "frequency"    , Sound_getFrequency , 0 );
    rb_define_method( cSound, "frequency="   , Sound_setFrequency , 1 );
    rb_define_method( cSound, "start="       , Sound_setStart     , 1 );
    rb_define_method( cSound, "loop_start="  , Sound_setLoopStart , 1 );
    rb_define_method( cSound, "loopStart="   , Sound_setLoopStart , 1 );
    rb_define_method( cSound, "loop_end="    , Sound_setLoopEnd   , 1 );
    rb_define_method( cSound, "loopEnd="     , Sound_setLoopEnd   , 1 );
    rb_define_method( cSound, "loop_count="  , Sound_setLoopCount , 1 );
    rb_define_method( cSound, "loopCount="   , Sound_setLoopCount , 1 );

    /* Sound�I�u�W�F�N�g�𐶐���������initialize�̑O�ɌĂ΂�郁�������蓖�Ċ֐��o�^ */
    rb_define_alloc_func( cSound, Sound_allocate );


    /* SoundEffect�N���X��` */
    cSoundEffect = rb_define_class_under( mDXRuby, "SoundEffect", rb_cObject );

    /* SoundEffect�N���X�Ƀ��\�b�h�o�^*/
    rb_define_private_method( cSoundEffect, "initialize", SoundEffect_initialize, -1 );
    rb_define_method( cSoundEffect, "dispose"   , SoundEffect_dispose   , 0 );
    rb_define_method( cSoundEffect, "disposed?" , SoundEffect_check_disposed, 0 );
    rb_define_method( cSoundEffect, "play"      , SoundEffect_play      , -1 );
    rb_define_method( cSoundEffect, "stop"      , SoundEffect_stop      , 0 );
    rb_define_method( cSoundEffect, "add"       , SoundEffect_add       , -1 );
    rb_define_method( cSoundEffect, "save"      , SoundEffect_save      , 1 );
#ifdef DXRUBY15
    rb_define_method( cSoundEffect, "to_a"      , SoundEffect_to_a      , 0 );
#endif
    /* SoundEffect�I�u�W�F�N�g�𐶐���������initialize�̑O�ɌĂ΂�郁�������蓖�Ċ֐��o�^ */
    rb_define_alloc_func( cSoundEffect, SoundEffect_allocate );

    rb_define_const( mDXRuby, "WAVE_SIN"     , INT2FIX(WAVE_SIN) );
    rb_define_const( mDXRuby, "WAVE_SAW"     , INT2FIX(WAVE_SAW) );
    rb_define_const( mDXRuby, "WAVE_TRI"     , INT2FIX(WAVE_TRI) );
    rb_define_const( mDXRuby, "WAVE_RECT"    , INT2FIX(WAVE_RECT) );

    rb_define_const( mDXRuby, "TYPE_MIDI"    , INT2FIX(0) );
    rb_define_const( mDXRuby, "TYPE_WAV"     , INT2FIX(1) );
}

