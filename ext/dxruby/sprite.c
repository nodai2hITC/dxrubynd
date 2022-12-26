#define WINVER 0x0500                                  /* �o�[�W������` Windows2000�ȏ� */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "sprite.h"
#include "image.h"
#include "collision.h"
#ifdef DXRUBY15
#include "matrix.h"
#endif

static VALUE cSprite;         /* �X�v���C�g�N���X       */
#ifdef DXRUBY15
extern VALUE cVector;
#endif

#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Image_data_type;
extern rb_data_type_t RenderTarget_data_type;
extern rb_data_type_t Shader_data_type;
#ifdef DXRUBY15
extern rb_data_type_t Matrix_data_type;
extern rb_data_type_t Vector_data_type;
#endif
#endif

ID id_shot;
ID id_hit;
ID id_update;
ID id_draw;
ID id_render;
ID id_vanished;
ID id_visible;

static VALUE Sprite_check( int argc, VALUE *argv, VALUE klass );

/*********************************************************************
 * Sprite�N���X
 *
 * �`��v���~�e�B�u�Ƃ��Ē񋟂���X�v���C�g�@�\
 *********************************************************************/
/*--------------------------------------------------------------------
   �Q�Ƃ���Ȃ��Ȃ����Ƃ���GC����Ă΂��֐�
 ---------------------------------------------------------------------*/
void Sprite_release( struct DXRubySprite *sprite )
{
    free( sprite );
}

/*--------------------------------------------------------------------
   Sprite�N���X��mark
 ---------------------------------------------------------------------*/
static void Sprite_mark( struct DXRubySprite *sprite )
{
    rb_gc_mark( sprite->vx );
    rb_gc_mark( sprite->vy );
    rb_gc_mark( sprite->vz );
    rb_gc_mark( sprite->vimage );
    rb_gc_mark( sprite->vtarget );
    rb_gc_mark( sprite->vangle );
    rb_gc_mark( sprite->vscale_x );
    rb_gc_mark( sprite->vscale_y );
    rb_gc_mark( sprite->vcenter_x );
    rb_gc_mark( sprite->vcenter_y );
    rb_gc_mark( sprite->valpha );
    rb_gc_mark( sprite->vblend );
    rb_gc_mark( sprite->vvisible );
    rb_gc_mark( sprite->vshader );
    rb_gc_mark( sprite->vcollision );
    rb_gc_mark( sprite->vcollision_enable );
    rb_gc_mark( sprite->vcollision_sync );
#ifdef DXRUBY15
    rb_gc_mark( sprite->vxy );
    rb_gc_mark( sprite->vxyz );
#endif
    rb_gc_mark( sprite->voffset_sync );
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Sprite_data_type = {
    "Sprite",
    {
    Sprite_mark,
    Sprite_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Sprite�N���X��allocate�B���������m�ۂ���ׂ�initialize�O�ɌĂ΂��B
 ---------------------------------------------------------------------*/
static VALUE Sprite_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubySprite *sprite;

    /* DXRubySprite�̃������擾��Sprite�I�u�W�F�N�g���� */
    sprite = malloc( sizeof(struct DXRubySprite) );
    if( sprite == NULL ) rb_raise( eDXRubyError, "Out of memory - Sprite_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Sprite_data_type, sprite );
#else
    obj = Data_Wrap_Struct(klass, Sprite_mark, Sprite_release, sprite);
#endif
    sprite->vx = INT2FIX( 0 );
    sprite->vy = INT2FIX( 0 );
    sprite->vz = INT2FIX( 0 );
    sprite->vimage = Qnil;
    sprite->vtarget = Qnil;
    sprite->vangle = INT2FIX( 0 );
    sprite->vscale_x = INT2FIX( 1 );
    sprite->vscale_y = INT2FIX( 1 );
    sprite->vcenter_x = Qnil;
    sprite->vcenter_y = Qnil;
    sprite->valpha = INT2FIX( 255 );
    sprite->vblend = Qnil;
    sprite->vvisible = Qtrue;
    sprite->vshader = Qnil;
    sprite->vcollision = Qnil;
    sprite->vcollision_enable = Qtrue;
    sprite->vcollision_sync = Qtrue;
    sprite->vanish = FALSE;
#ifdef DXRUBY15
    sprite->vxy = Qnil;
    sprite->vxyz = Qnil;
#endif
    sprite->voffset_sync = Qfalse;

    return obj;
}


/*--------------------------------------------------------------------
   Sprite�N���X��Initialize
 ---------------------------------------------------------------------*/
static VALUE Sprite_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );

    sprite->vx     = (argc == 0 || argv[0] == Qnil) ? INT2FIX( 0 ) : argv[0];
    sprite->vy     = (argc < 2  || argv[1] == Qnil) ? INT2FIX( 0 ) : argv[1];
    sprite->vimage = (argc < 3) ? Qnil : argv[2];

    return self;
}


/*--------------------------------------------------------------------
   Sprite�N���X��draw
 ---------------------------------------------------------------------*/
void Sprite_internal_draw( VALUE self, VALUE vrt )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, vrt );
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    struct DXRubyImage *image;
    float z;

    if( !RTEST(sprite->vvisible) || sprite->vanish )
    {
        return;
    }

    /* �C���[�W�I�u�W�F�N�g���璆�g�����o�� */
    if( sprite->vimage == Qnil )
    {
        return;
    }

    DXRUBY_CHECK_IMAGE( sprite->vimage );
    image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
    DXRUBY_CHECK_DISPOSE( image, texture );

    if( sprite->vangle == INT2FIX( 0 ) && sprite->vscale_x == INT2FIX( 1 ) && sprite->vscale_y == INT2FIX( 1 ) )
    {/* ��]�g�喳�� */
        volatile VALUE temp;
        struct DXRubyPicture_draw *picture;

        picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

        /* DXRubyPicture�I�u�W�F�N�g�ݒ� */
        picture->x = (int)(NUM2FLOAT( sprite->vx ) - (RTEST(sprite->voffset_sync) ? (sprite->vcenter_x == Qnil ? image->width / 2.0f : NUM2FLOAT( sprite->vcenter_x )) : 0));
        picture->y = (int)(NUM2FLOAT( sprite->vy ) - (RTEST(sprite->voffset_sync) ? (sprite->vcenter_y == Qnil ? image->height / 2.0f : NUM2FLOAT( sprite->vcenter_y )) : 0));
        picture->x -= rt->ox;
        picture->y -= rt->oy;

        if( sprite->vshader == Qnil )
        {/* �V�F�[�_�Ȃ� */
            picture->func = RenderTarget_draw_func;
            picture->value = sprite->vimage;
        }
        else
        {/* �V�F�[�_���� */
            struct DXRubyShader *shader;
            struct DXRubyShaderCore *core;

            DXRUBY_CHECK_TYPE( Shader, sprite->vshader );
            shader = DXRUBY_GET_STRUCT( Shader, sprite->vshader );
            core = DXRUBY_GET_STRUCT( ShaderCore, shader->vcore );
            DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );

            temp = rb_ary_new3( 3, sprite->vimage, shader->vcore, shader->vparam );
            picture->value = temp;

            /* Shader����Image�I�u�W�F�N�g�����b�N���� */
//            rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach_lock, RARRAY_PTR( picture->value )[1]);

            picture->func = RenderTarget_drawShader_func;
        }

        picture->alpha = NUM2INT( sprite->valpha );
        picture->blendflag = (sprite->vblend == Qnil ? 0 :
                             (sprite->vblend == symbol_add ? 4 :
                             (sprite->vblend == symbol_none ? 1 :
                             (sprite->vblend == symbol_add2 ? 5 :
                             (sprite->vblend == symbol_sub ? 6 :
                             (sprite->vblend == symbol_sub2 ? 7 : 0))))));

        /* ���X�g�f�[�^�ɒǉ� */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = NUM2FLOAT( sprite->vz );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;
    }
    else
    {
        volatile VALUE temp;
        struct DXRubyPicture_drawEx *picture;

        picture = (struct DXRubyPicture_drawEx *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawEx ) );

        /* DXRubyPicture�I�u�W�F�N�g�ݒ� */
        picture->func = RenderTarget_drawEx_func;
        picture->x = (int)(NUM2FLOAT( sprite->vx ) - (RTEST(sprite->voffset_sync) ? (sprite->vcenter_x == Qnil ? image->width / 2.0f : NUM2FLOAT( sprite->vcenter_x )) : 0));
        picture->y = (int)(NUM2FLOAT( sprite->vy ) - (RTEST(sprite->voffset_sync) ? (sprite->vcenter_y == Qnil ? image->height / 2.0f : NUM2FLOAT( sprite->vcenter_y )) : 0));
        picture->x -= rt->ox;
        picture->y -= rt->oy;

        if( sprite->vshader != Qnil )
        {/* �V�F�[�_���� */
            struct DXRubyShader *shader;
            struct DXRubyShaderCore *core;
            DXRUBY_CHECK_TYPE( Shader, sprite->vshader );
            shader = DXRUBY_GET_STRUCT( Shader, sprite->vshader );
            core = DXRUBY_GET_STRUCT( ShaderCore, shader->vcore );
            DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );

            temp = rb_ary_new3( 3, sprite->vimage, shader->vcore, shader->vparam );
            picture->value = temp;

            /* Shader����Image�I�u�W�F�N�g�����b�N���� */
//            rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach_lock, RARRAY_PTR( picture->value )[1]);
        }
        else
        {
            picture->value = sprite->vimage;
        }

        picture->angle   = NUM2FLOAT( sprite->vangle );
        picture->scalex  = NUM2FLOAT( sprite->vscale_x );
        picture->scaley  = NUM2FLOAT( sprite->vscale_y );
        picture->centerx = (sprite->vcenter_x == Qnil ? image->width / 2.0f : NUM2FLOAT( sprite->vcenter_x ));
        picture->centery = (sprite->vcenter_y == Qnil ? image->height / 2.0f : NUM2FLOAT( sprite->vcenter_y ));

        picture->alpha   = NUM2INT( sprite->valpha );
        picture->blendflag = (sprite->vblend == Qnil ? 0 :
                             (sprite->vblend == symbol_add ? 4 :
                             (sprite->vblend == symbol_none ? 1 :
                             (sprite->vblend == symbol_add2 ? 5 :
                             (sprite->vblend == symbol_sub ? 6 :
                             (sprite->vblend == symbol_sub2 ? 7 : 0))))));

        /* ���X�g�f�[�^�ɒǉ� */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = NUM2FLOAT( sprite->vz );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;
    }

    /* RenderTarget�������ꍇ�ɕ`��\�񂪂����update���� */
    if( DXRUBY_CHECK( RenderTarget, sprite->vimage ) )
    {
        struct DXRubyRenderTarget *src_rt = DXRUBY_GET_STRUCT( RenderTarget, sprite->vimage );

        if( src_rt->clearflag == 0 && src_rt->PictureCount == 0 )
        {
            g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, src_rt->surface );
            g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                         D3DCOLOR_ARGB( src_rt->a, src_rt->r, src_rt->g, src_rt->b ), 1.0f, 0 );
            src_rt->clearflag = 1;
        }
        else if( src_rt->PictureCount > 0 )
        {
            RenderTarget_update( sprite->vimage );
        }
    }

    /* �g��ꂽimage�̃��b�N */
//    image->lockcount += 1;
}


VALUE Sprite_draw( VALUE self )
{
    VALUE vrt;
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );

    if( sprite->vtarget == Qnil || sprite->vtarget == mWindow )
    {
        vrt = g_WindowInfo.render_target;
    }
    else
    {
        DXRUBY_CHECK_TYPE( RenderTarget, sprite->vtarget );
        vrt = sprite->vtarget;
    }

    Sprite_internal_draw( self, vrt );

    return self;
}

/*--------------------------------------------------------------------
   �v���p�e�B��Setter/Getter
 ---------------------------------------------------------------------*/
/* x */
static VALUE Sprite_get_x( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vx;
}

/* x= */
static VALUE Sprite_set_x( VALUE self, VALUE vx )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vx = vx;
    return vx;
}

/* y */
static VALUE Sprite_get_y( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vy;
}

/* y= */
static VALUE Sprite_set_y( VALUE self, VALUE vy )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vy = vy;
    return vy;
}

/* z */
static VALUE Sprite_get_z( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vz;
}

/* z= */
static VALUE Sprite_set_z( VALUE self, VALUE vz )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vz = vz;
    return vz;
}

/* angle */
static VALUE Sprite_get_angle( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vangle;
}

/* angle= */
static VALUE Sprite_set_angle( VALUE self, VALUE vangle )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vangle = vangle;
    return vangle;
}


/* image */
static VALUE Sprite_get_image( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vimage;
}

/* image= */
static VALUE Sprite_set_image( VALUE self, VALUE vimage )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vimage = vimage;
    return vimage;
}

/* blend */
static VALUE Sprite_get_blend( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vblend;
}

/* blend= */
static VALUE Sprite_set_blend( VALUE self, VALUE vblend )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vblend = vblend;
    return vblend;
}

/* alpha */
static VALUE Sprite_get_alpha( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->valpha;
}

/* alpha= */
static VALUE Sprite_set_alpha( VALUE self, VALUE valpha )
{
    DXRUBY_GET_STRUCT( Sprite, self )->valpha = valpha;
    return valpha;
}

/* target */
static VALUE Sprite_get_target( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vtarget;
}

/* target= */
static VALUE Sprite_set_target( VALUE self, VALUE vtarget )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vtarget = vtarget;
    return vtarget;
}

/* shader */
static VALUE Sprite_get_shader( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vshader;
}

/* shader= */
static VALUE Sprite_set_shader( VALUE self, VALUE vshader )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vshader = vshader;
    return vshader;
}

/* scale_x */
static VALUE Sprite_get_scale_x( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vscale_x;
}

/* scale_x= */
static VALUE Sprite_set_scale_x( VALUE self, VALUE vscale_x )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vscale_x = vscale_x;
    return vscale_x;
}

/* scale_y */
static VALUE Sprite_get_scale_y( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vscale_y;
}

/* scale_y= */
static VALUE Sprite_set_scale_y( VALUE self, VALUE vscale_y )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vscale_y = vscale_y;
    return vscale_y;
}

/* center_x */
static VALUE Sprite_get_center_x( VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    if( sprite->vcenter_x == Qnil )
    {
        struct DXRubyImage *image;

        if( sprite->vimage == Qnil )
        {
            return Qnil;
        }

        DXRUBY_CHECK_IMAGE( sprite->vimage );
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
        DXRUBY_CHECK_DISPOSE( image, texture );
        return rb_float_new( image->width / 2.0f );
    }
    else
    {
        return sprite->vcenter_x;
    }
}

/* center_x= */
static VALUE Sprite_set_center_x( VALUE self, VALUE vcenter_x )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vcenter_x = vcenter_x;
    return vcenter_x;
}

/* center_y */
static VALUE Sprite_get_center_y( VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    if( sprite->vcenter_y == Qnil )
    {
        struct DXRubyImage *image;

        if( sprite->vimage == Qnil )
        {
            return Qnil;
        }

        DXRUBY_CHECK_IMAGE( sprite->vimage );
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
        DXRUBY_CHECK_DISPOSE( image, texture );
        return rb_float_new( image->height / 2.0f );
    }
    else
    {
        return sprite->vcenter_y;
    }
}

/* center_y= */
static VALUE Sprite_set_center_y( VALUE self, VALUE vcenter_y )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vcenter_y = vcenter_y;
    return vcenter_y;
}

/* collision_enable */
static VALUE Sprite_get_collision_enable( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vcollision_enable;
}

/* collision_enable= */
static VALUE Sprite_set_collision_enable( VALUE self, VALUE vcollision_enable )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vcollision_enable = vcollision_enable;
    return vcollision_enable;
}

/* collision_sync */
static VALUE Sprite_get_collision_sync( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vcollision_sync;
}

/* collision_sync= */
static VALUE Sprite_set_collision_sync( VALUE self, VALUE vcollision_sync )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vcollision_sync = vcollision_sync;
    return vcollision_sync;
}

/* collision */
static VALUE Sprite_get_collision( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vcollision;
//    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
//    struct DXRubyCollision temp;
//    make_volume( rb_ary_new3(1, self), &temp );
//    return rb_ary_new3( 4, rb_float_new(temp.x1), rb_float_new(temp.y1), rb_float_new(temp.x2), rb_float_new(temp.y2) );
}

/* collision= */
static VALUE Sprite_set_collision( VALUE self, VALUE vcollision )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vcollision = vcollision;
    return vcollision;
}

/* visible */
static VALUE Sprite_get_visible( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vvisible;
}

/* visible= */
static VALUE Sprite_set_visible( VALUE self, VALUE vvisible )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vvisible = vvisible;
    return vvisible;
}

/* param_hash */
static VALUE Sprite_get_param_hash( VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    VALUE vresult;

    vresult = rb_hash_new();
    rb_hash_aset( vresult, symbol_angle, sprite->vangle );
    rb_hash_aset( vresult, symbol_alpha, sprite->valpha );
    rb_hash_aset( vresult, symbol_z, sprite->vz );
#ifdef DXRUBY15
    rb_hash_aset( vresult, symbol_scale_x, sprite->vscale_x );
    rb_hash_aset( vresult, symbol_scale_y, sprite->vscale_y );
    rb_hash_aset( vresult, symbol_center_x, sprite->vcenter_x );
    rb_hash_aset( vresult, symbol_center_y, sprite->vcenter_y );
#else
    rb_hash_aset( vresult, symbol_scalex, sprite->vscale_x );
    rb_hash_aset( vresult, symbol_scaley, sprite->vscale_y );
    rb_hash_aset( vresult, symbol_centerx, sprite->vcenter_x );
    rb_hash_aset( vresult, symbol_centery, sprite->vcenter_y );
#endif
    rb_hash_aset( vresult, symbol_shader, sprite->vshader );
    rb_hash_aset( vresult, symbol_blend, sprite->vblend );
    rb_hash_aset( vresult, symbol_offset_sync, sprite->voffset_sync );

    return vresult;
}

/* vanish */
static VALUE Sprite_vanish( VALUE self )
{
    DXRUBY_GET_STRUCT( Sprite, self )->vanish = TRUE;
    return self;
}

/* vanish? */
static VALUE Sprite_get_vanish( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->vanish ? Qtrue : Qfalse;
}

/* update */
static VALUE Sprite_update( VALUE self )
{
    return self;
}

/* offset_sync */
static VALUE Sprite_get_offset_sync( VALUE self )
{
    return DXRUBY_GET_STRUCT( Sprite, self )->voffset_sync;
}

/* offset_sync= */
static VALUE Sprite_set_offset_sync( VALUE self, VALUE voffset_sync )
{
    DXRUBY_GET_STRUCT( Sprite, self )->voffset_sync = voffset_sync;
    return voffset_sync;
}

#ifdef DXRUBY15
/* xy */
static VALUE Sprite_get_xy( VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float sx, sy;
    sx = NUM2FLOAT( sprite->vx );
    sy = NUM2FLOAT( sprite->vy );

    if( sprite->vxy != Qnil )
    {
        float x, y;
        DXRUBY_CHECK_TYPE( Vector, sprite->vxy );
        x = DXRUBY_GET_STRUCT( Vector, sprite->vxy )->v1;
        y = DXRUBY_GET_STRUCT( Vector, sprite->vxy )->v2;

        if( sx == x && sy == y )
        {
            return sprite->vxy;
        }
    }

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = 2;
    result->v1 = sx;
    result->v2 = sy;
    sprite->vxy = vresult;

    return vresult;
}

/* xy= */
static VALUE Sprite_set_xy( VALUE self, VALUE vxy )
{
    struct DXRubyVector *vec;

    DXRUBY_CHECK_TYPE( Vector, vxy );
    vec = DXRUBY_GET_STRUCT( Vector, vxy );

    DXRUBY_GET_STRUCT( Sprite, self )->vxy = vxy;
    DXRUBY_GET_STRUCT( Sprite, self )->vx = rb_float_new( vec->v1 );
    DXRUBY_GET_STRUCT( Sprite, self )->vy = rb_float_new( vec->v2 );

    return vxy;
}

/* xyz */
static VALUE Sprite_get_xyz( VALUE self )
{
    struct DXRubySprite *sprite = DXRUBY_GET_STRUCT( Sprite, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float sx, sy, sz;
    sx = NUM2FLOAT( sprite->vx );
    sy = NUM2FLOAT( sprite->vy );
    sz = NUM2FLOAT( sprite->vz );

    if( sprite->vxyz != Qnil )
    {
        float x, y, z;
        DXRUBY_CHECK_TYPE( Vector, sprite->vxyz );
        x = DXRUBY_GET_STRUCT( Vector, sprite->vxyz )->v1;
        y = DXRUBY_GET_STRUCT( Vector, sprite->vxyz )->v2;
        z = DXRUBY_GET_STRUCT( Vector, sprite->vxyz )->v3;

        if( sx == x && sy == y && sz == z)
        {
            return sprite->vxyz;
        }
    }

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = 3;
    result->v1 = sx;
    result->v2 = sy;
    result->v3 = sz;
    sprite->vxyz = vresult;

    return vresult;
}


/* xyz= */
static VALUE Sprite_set_xyz( VALUE self, VALUE vxyz )
{
    struct DXRubyVector *vec;

    DXRUBY_CHECK_TYPE( Vector, vxyz );
    vec = DXRUBY_GET_STRUCT( Vector, vxyz );

    DXRUBY_GET_STRUCT( Sprite, self )->vxyz = vxyz;
    DXRUBY_GET_STRUCT( Sprite, self )->vx = rb_float_new( vec->v1 );
    DXRUBY_GET_STRUCT( Sprite, self )->vy = rb_float_new( vec->v2 );
    DXRUBY_GET_STRUCT( Sprite, self )->vz = rb_float_new( vec->v3 );

    return vxyz;
}
#endif

/*--------------------------------------------------------------------
   �P�̂Ɣz��̔���
 ---------------------------------------------------------------------*/
static VALUE Sprite_hitcheck( VALUE self, VALUE vsprite )
{
    struct DXRubyCollisionGroup collision1;
    VALUE vary;

    /* �Փ˂����I�u�W�F�N�g������z�� */
    vary = rb_ary_new();

    /* self��AABB�{�����[���v�Z */
    if( make_volume( self, &collision1 ) == 0 )
    {
        collision_clear();
        return vary;
    }

    if( TYPE(vsprite) == T_ARRAY )
    {
        struct DXRubyCollisionGroup *collision2;
        int i, d_total;

        /* �Ώۂ�AABB�{�����[���v�Z */
        collision2 = (struct DXRubyCollisionGroup *)malloc( get_volume_count( vsprite ) * sizeof(struct DXRubyCollisionGroup) );
        d_total = make_volume_ary( vsprite, collision2 );

        /* �ΏۃI�u�W�F�N�g�̃��[�v */
        for( i = 0; i < d_total; i++ )
        {
            /* ���� */
            if( check_box_box( &collision1, collision2 + i ) && check( &collision1, collision2 + i ) )
            {
                rb_ary_push( vary, (collision2 + i)->vsprite );
            }
        }

        free( collision2 );
    }
    else
    {
        struct DXRubyCollisionGroup collision2;

        /* �Ώۂ�AABB�{�����[���v�Z */
        if( make_volume( vsprite, &collision2 ) > 0 )
        {
            /* ���� */
            if( check_box_box( &collision1, &collision2 ) && check( &collision1, &collision2 ) )
            {
                rb_ary_push( vary, collision2.vsprite );
            }
        }
    }

    collision_clear();
    return vary;
}


/*--------------------------------------------------------------------
   �P�̂̔���
 ---------------------------------------------------------------------*/
static VALUE Sprite_compare( VALUE self, VALUE vsprite )
{
    struct DXRubyCollisionGroup collision1;
    VALUE vary;

    /* �Փ˂����I�u�W�F�N�g������z�� */
    vary = rb_ary_new();

    /* self��AABB�{�����[���v�Z */
    if( make_volume( self, &collision1 ) == 0 )
    {
        collision_clear();
        return vary;
    }

    if( TYPE(vsprite) == T_ARRAY )
    {
        struct DXRubyCollisionGroup *collision2;
        int i, d_total;

        /* �Ώۂ�AABB�{�����[���v�Z */
        collision2 = (struct DXRubyCollisionGroup *)malloc( get_volume_count( vsprite ) * sizeof(struct DXRubyCollisionGroup) );
        d_total = make_volume_ary( vsprite, collision2 );

        /* �ΏۃI�u�W�F�N�g�̃��[�v */
        for( i = 0; i < d_total; i++ )
        {
            /* ���� */
            if( check_box_box( &collision1, collision2 + i ) && check( &collision1, collision2 + i ) )
            {
                free( collision2 );
                collision_clear();
                return Qtrue;
            }
        }

        free( collision2 );
    }
    else
    {
        struct DXRubyCollisionGroup collision2;

        /* �Ώۂ�AABB�{�����[���v�Z */
        if( make_volume( vsprite, &collision2 ) > 0 )
        {
            /* ���� */
            if( check_box_box( &collision1, &collision2 ) && check( &collision1, &collision2 ) )
            {
                collision_clear();
                return Qtrue;
            }
        }
    }

    collision_clear();
    return Qfalse;
}


/*--------------------------------------------------------------------
   �z�񓯎m�̔���
 ---------------------------------------------------------------------*/
static VALUE Sprite_check( int argc, VALUE *argv, VALUE klass )
{
    int i, j;
    int flag = 0;           /* �ʃO���[�v�Ȃ�0�A�����O���[�v�Ȃ�1 */
    VALUE hitflag = Qfalse; /* ��ł�����������Qtrue */
    VALUE o, d;
    ID id_shot_temp, id_hit_temp;
    int shot_flg = 0, hit_flg = 0;
    VALUE ary;
    int ary_count = 0;
    struct DXRubyCollisionGroup *collision1;
    struct DXRubyCollisionGroup *collision2;
    int o_total, d_total;

    if( argc < 1 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 1, 4 );

    o = argv[0];

    if( argc == 1 )
    {
        d = o;
    }
    else
    {
        d = argv[1];
    }

    /* �P�̎w��̏ꍇ�͂Ƃ肠�����z��ɓ˂����� */
    if( TYPE(o) != T_ARRAY )
    {
        o = rb_ary_new3( 1, o );
    }
    if( TYPE(d) != T_ARRAY )
    {
        d = rb_ary_new3( 1, d );
    }

    /* �����z�񂾂�����flag�𗧂Ă� */
    if( o == d )
    {
        flag = 1;
        id_shot_temp = id_hit;
        id_hit_temp = id_hit;
    }
    else
    {
        id_shot_temp = id_shot;
        id_hit_temp = id_hit;
    }

    /* �Ăѐ惁�\�b�h */
    if( argc > 2 )
    {
        if( TYPE(argv[2]) == T_STRING )
        {
            id_shot_temp = rb_intern( RSTRING_PTR(argv[2]) );
        }
        else if( TYPE(argv[2]) == T_SYMBOL )
        {
            id_shot_temp = SYM2ID( argv[2] );
        }
        else if( argv[2] == Qnil )
        {
            shot_flg = 1;
        }
        else
        {
            rb_raise( rb_eTypeError, "wrong argument type %s (expected String or Symbol)", rb_obj_classname( argv[2] ) );
        }
    }

    if( argc > 3 )
    {
        if( TYPE(argv[3]) == T_STRING )
        {
            id_hit_temp = rb_intern( RSTRING_PTR(argv[3]) );
        }
        else if( TYPE(argv[3]) == T_SYMBOL )
        {
            id_hit_temp = SYM2ID( argv[3] );
        }
        else if( argv[3] == Qnil )
        {
            hit_flg = 1;
        }
        else
        {
            rb_raise( rb_eTypeError, "wrong argument type %s (expected String or Symbol)", rb_obj_classname( argv[3] ) );
        }
    }

    /* AABB�{�����[���v�Z */
    collision1 = (struct DXRubyCollisionGroup *)malloc( get_volume_count( o ) * sizeof(struct DXRubyCollisionGroup) );
    o_total = make_volume_ary( o, collision1 );

    if( flag == 0 )
    {
        collision2 = (struct DXRubyCollisionGroup *)malloc( get_volume_count( d ) * sizeof(struct DXRubyCollisionGroup) );
        d_total = make_volume_ary( d, collision2 );
    }
    else
    {
        d_total = o_total;
        collision2 = collision1;
    }

    /* �Փ˂��m�点�Ώ� */
//    ary = (VALUE *)alloca( o_total * d_total * 2 * sizeof(VALUE *) );
    ary = rb_ary_new();

    /* �U�����I�u�W�F�N�g�̃��[�v */
    for( i = 0; i < o_total - flag; i++ )
    {
        /* �h�䑤�I�u�W�F�N�g�̃��[�v */
        for( j = (flag == 0 ? 0 : i + 1); j < d_total; j++ )
        {
            /* ���� */
            if( check_box_box( collision1 + i, collision2 + j ) && check( collision1 + i, collision2 + j ) )
            {
                hitflag = Qtrue;
                rb_ary_push( ary, (collision1 + i)->vsprite );
                rb_ary_push( ary, (collision2 + j)->vsprite );
//                *(ary + ary_count) = (collision1 + i)->vsprite;
//                *(ary + ary_count + 1) = (collision2 + j)->vsprite;
                ary_count += 2;
            }
        }
    }

    /* �Փ˂��m�点�R�[���o�b�N */
    for( i = 0; i < ary_count; i += 2 )
    {
        VALUE result;
        if( DXRUBY_GET_STRUCT(Sprite, RARRAY_AREF(ary, i))->vanish || DXRUBY_GET_STRUCT(Sprite, RARRAY_AREF(ary, i + 1))->vanish )
        {
            continue;
        }
        if( flag == 0 )
        {
            if( rb_respond_to( RARRAY_AREF(ary, i), id_shot_temp ) && !shot_flg )
            {
                if( rb_obj_method_arity( RARRAY_AREF(ary, i), id_shot_temp) == 0 ) /* ���������� */
                {
                    result = rb_funcall( RARRAY_AREF(ary, i), id_shot_temp, 0 );
                }
                else /* ���������� */
                {
                    result = rb_funcall( RARRAY_AREF(ary, i), id_shot_temp, 1, RARRAY_AREF(ary, i + 1) );
                }

                if( result == symbol_discard )
                { /* ��������Ȃ� */
                    int j;
                    for( j = i + 2; j < ary_count; j += 2) /* ����Sprite�������׍H���� */
                    {
                        if( RARRAY_AREF(ary, i) == RARRAY_AREF(ary, j) || RARRAY_AREF(ary, i) == RARRAY_AREF(ary, j + 1) ) /* �����I�u�W�F�N�g�������� */
                        {
                            RARRAY_ASET(ary, j, Qnil); /* ���� */
                            RARRAY_ASET(ary, j + 1, Qnil);
                        }
                    }
                }
            }
        }
        else
        {
            if( rb_respond_to( RARRAY_AREF(ary, i), id_hit_temp ) && !hit_flg )
            {
                if( rb_obj_method_arity( RARRAY_AREF(ary, i), id_hit_temp) == 0 ) /* ���������� */
                {
                    result = rb_funcall( RARRAY_AREF(ary, i), id_hit_temp, 0 );
                }
                else /* ���������� */
                {
                    result = rb_funcall( RARRAY_AREF(ary, i), id_hit_temp, 1, RARRAY_AREF(ary, i + 1) );
                }

                if( result == symbol_discard )
                { /* ��������Ȃ� */
                    int j;
                    for( j = i + 2; j < ary_count; j += 2) /* ����Sprite�������׍H���� */
                    {
                        if( RARRAY_AREF(ary, i) == RARRAY_AREF(ary, j) || RARRAY_AREF(ary, i) == RARRAY_AREF(ary, j + 1) ) /* �����I�u�W�F�N�g�������� */
                        {
                            RARRAY_ASET(ary, j, Qnil); /* ���� */
                            RARRAY_ASET(ary, j + 1, Qnil);
                        }
                    }
                }
            }
        }

        if( rb_respond_to( RARRAY_AREF(ary, i + 1), id_hit_temp ) && !hit_flg )
        {
            if( rb_obj_method_arity( RARRAY_AREF(ary, i + 1), id_hit_temp) == 0 ) /* ���������� */
            {
                result = rb_funcall( RARRAY_AREF(ary, i + 1), id_hit_temp, 0 );
            }
            else /* ���������� */
            {
                result = rb_funcall( RARRAY_AREF(ary, i + 1), id_hit_temp, 1, RARRAY_AREF(ary, i) );
            }

            if( result == symbol_discard )
            { /* ��������Ȃ� */
                int j;
                for( j = i + 2; j < ary_count; j += 2) /* ����Sprite�������׍H���� */
                {
                    if( RARRAY_AREF(ary, i + 1) == RARRAY_AREF(ary, j) || RARRAY_AREF(ary, i + 1) == RARRAY_AREF(ary, j + 1) ) /* �����I�u�W�F�N�g�������� */
                    {
                        RARRAY_ASET(ary, j, Qnil); /* ���� */
                        RARRAY_ASET(ary, j + 1, Qnil);
                    }
                }
            }
        }
    }

    free( collision1 );
    if( flag == 0 )
    {
        free( collision2 );
    }

    collision_clear();

    return hitflag;
}


static VALUE Sprite_class_update( VALUE klass, VALUE ary )
{
    int i;

    if( TYPE( ary ) != T_ARRAY )
    {
        ary = rb_ary_new3( 1, ary );
    }

    for( i = 0; i < RARRAY_LEN( ary ); i++ )
    {
        VALUE p = RARRAY_AREF( ary, i );

        if( TYPE( p ) == T_ARRAY )
        {
            Sprite_class_update( cSprite, p );
        }
        else if( !rb_respond_to( p, id_vanished ) || !RTEST( rb_funcall2( p, id_vanished, 0, 0 ) ) )
        {
            if( rb_respond_to( p, id_update ) )
            {
                rb_funcall2( p, id_update, 0, 0 );
            }
        }
    }
    return Qnil;
}


static VALUE Sprite_class_draw( VALUE klass, VALUE ary )
{
    int i;

    if( TYPE( ary ) != T_ARRAY )
    {
        ary = rb_ary_new3( 1, ary );
    }

    for( i = 0; i < RARRAY_LEN( ary ); i++ )
    {
        VALUE p = RARRAY_AREF( ary, i );

        if( TYPE( p ) == T_ARRAY )
        {
            Sprite_class_draw( cSprite, p );
        }
        else if( !rb_respond_to( p, id_vanished ) || !RTEST( rb_funcall2( p, id_vanished, 0, 0 ) ) )
        {
            if( rb_respond_to( p, id_draw ) )
            {
                rb_funcall2( p, id_draw, 0, 0 );
            }
        }
    }
    return Qnil;
}


static VALUE Sprite_class_render( VALUE klass, VALUE ary )
{
    int i;

    if( TYPE( ary ) != T_ARRAY )
    {
        ary = rb_ary_new3( 1, ary );
    }

    for( i = 0; i < RARRAY_LEN( ary ); i++ )
    {
        VALUE p = RARRAY_AREF( ary, i );

        if( TYPE( p ) == T_ARRAY )
        {
            Sprite_class_draw( cSprite, p );
        }
        else if( !rb_respond_to( p, id_vanished ) || !RTEST( rb_funcall2( p, id_vanished, 0, 0 ) ) )
        {
            if( rb_respond_to( p, id_render ) )
            {
                rb_funcall2( p, id_render, 0, 0 );
            }
        }
    }
    return Qnil;
}


static VALUE Sprite_class_clean( VALUE klass, VALUE ary )
{
    int i;

    if( TYPE( ary ) != T_ARRAY )
    {
        return Qnil;
    }

    for( i = 0; i < RARRAY_LEN( ary ); i++ )
    {
        VALUE p = RARRAY_AREF( ary, i );

        if( TYPE( p ) == T_ARRAY )
        {
            Sprite_class_clean( cSprite, p );
        }
        else if( rb_respond_to( p, id_vanished ) && RTEST( rb_funcall2( p, id_vanished, 0, 0 ) ) )
        {
            RARRAY_ASET( ary, i, Qnil);
        }
    }

    rb_funcall( ary, rb_intern("compact!"), 0 );

    return Qnil;
}


/*
***************************************************************
*
*         Global functions
*
***************************************************************/

void Init_dxruby_Sprite()
{
    /* Sprite�N���X��` */
    cSprite = rb_define_class_under( mDXRuby, "Sprite", rb_cObject );

    rb_define_private_method( cSprite, "initialize", Sprite_initialize, -1 );
    rb_define_method( cSprite, "x", Sprite_get_x, 0 );
    rb_define_method( cSprite, "x=", Sprite_set_x, 1 );
    rb_define_method( cSprite, "y", Sprite_get_y, 0 );
    rb_define_method( cSprite, "y=", Sprite_set_y, 1 );
    rb_define_method( cSprite, "z", Sprite_get_z, 0 );
    rb_define_method( cSprite, "z=", Sprite_set_z, 1 );
    rb_define_method( cSprite, "angle", Sprite_get_angle, 0 );
    rb_define_method( cSprite, "angle=", Sprite_set_angle, 1 );
    rb_define_method( cSprite, "scale_x", Sprite_get_scale_x, 0 );
    rb_define_method( cSprite, "scale_x=", Sprite_set_scale_x, 1 );
    rb_define_method( cSprite, "scale_y", Sprite_get_scale_y, 0 );
    rb_define_method( cSprite, "scale_y=", Sprite_set_scale_y, 1 );
    rb_define_method( cSprite, "center_x", Sprite_get_center_x, 0 );
    rb_define_method( cSprite, "center_x=", Sprite_set_center_x, 1 );
    rb_define_method( cSprite, "center_y", Sprite_get_center_y, 0 );
    rb_define_method( cSprite, "center_y=", Sprite_set_center_y, 1 );
    rb_define_method( cSprite, "alpha", Sprite_get_alpha, 0 );
    rb_define_method( cSprite, "alpha=", Sprite_set_alpha, 1 );
    rb_define_method( cSprite, "blend", Sprite_get_blend, 0 );
    rb_define_method( cSprite, "blend=", Sprite_set_blend, 1 );
    rb_define_method( cSprite, "image", Sprite_get_image, 0 );
    rb_define_method( cSprite, "image=", Sprite_set_image, 1 );
    rb_define_method( cSprite, "target", Sprite_get_target, 0 );
    rb_define_method( cSprite, "target=", Sprite_set_target, 1 );
    rb_define_method( cSprite, "shader", Sprite_get_shader, 0 );
    rb_define_method( cSprite, "shader=", Sprite_set_shader, 1 );
    rb_define_method( cSprite, "collision", Sprite_get_collision, 0 );
    rb_define_method( cSprite, "collision=", Sprite_set_collision, 1 );
    rb_define_method( cSprite, "collision_enable", Sprite_get_collision_enable, 0 );
    rb_define_method( cSprite, "collision_enable=", Sprite_set_collision_enable, 1 );
    rb_define_method( cSprite, "collision_sync", Sprite_get_collision_sync, 0 );
    rb_define_method( cSprite, "collision_sync=", Sprite_set_collision_sync, 1 );
    rb_define_method( cSprite, "visible", Sprite_get_visible, 0 );
    rb_define_method( cSprite, "visible=", Sprite_set_visible, 1 );
    rb_define_method( cSprite, "update", Sprite_update, 0 );
    rb_define_method( cSprite, "draw", Sprite_draw, 0 );
//    rb_define_method( cSprite, "render", Sprite_draw, 0 );
    rb_define_method( cSprite, "===", Sprite_compare, 1 );
    rb_define_method( cSprite, "check", Sprite_hitcheck, 1 );
    rb_define_method( cSprite, "param_hash", Sprite_get_param_hash, 0 );
    rb_define_method( cSprite, "vanish", Sprite_vanish, 0 );
    rb_define_method( cSprite, "vanished?", Sprite_get_vanish, 0 );
    rb_define_method( cSprite, "offset_sync", Sprite_get_offset_sync, 0 );
    rb_define_method( cSprite, "offset_sync=", Sprite_set_offset_sync, 1 );
#ifdef DXRUBY15
    rb_define_method( cSprite, "xy", Sprite_get_xy, 0 );
    rb_define_method( cSprite, "xy=", Sprite_set_xy, 1 );
    rb_define_method( cSprite, "xyz", Sprite_get_xyz, 0 );
    rb_define_method( cSprite, "xyz=", Sprite_set_xyz, 1 );
#endif

    /* Sprite�N���X�Ƀ��\�b�h�o�^ */
    rb_define_singleton_method( cSprite, "check", Sprite_check, -1 );
    rb_define_singleton_method( cSprite, "update", Sprite_class_update, 1 );
    rb_define_singleton_method( cSprite, "draw", Sprite_class_draw, 1 );
//    rb_define_singleton_method( cSprite, "render", Sprite_class_render, 1 );
    rb_define_singleton_method( cSprite, "clean", Sprite_class_clean, 1 );

    rb_define_alloc_func( cSprite, Sprite_allocate );

    id_shot = rb_intern("shot");
    id_hit = rb_intern("hit");
    id_update = rb_intern("update");
    id_draw = rb_intern("draw");
    id_render = rb_intern("render");
    id_vanished = rb_intern("vanished?");
    id_visible = rb_intern("visible");

    collision_init();
}

