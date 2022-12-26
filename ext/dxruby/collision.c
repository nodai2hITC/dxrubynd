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

static int g_volume_count;
static int g_volume_allocate_count;
static struct DXRubyCollision *g_volume_pointer;

#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Image_data_type;
extern rb_data_type_t RenderTarget_data_type;
extern rb_data_type_t Sprite_data_type;
#endif

/*********************************************************************
 * �Փ˔��菈��
 *
 *********************************************************************/

/* ���E�{�����[���쐬 */
#define volume_box( count, tx, ty, collision ) \
{\
    int i;\
    (collision)->x1 = (collision)->x2 = (int)tx[0];\
    (collision)->y1 = (collision)->y2 = (int)ty[0];\
\
    for( i = 1; i < count; i++ )\
    {\
        if( (collision)->x1 > (int)tx[i] )\
        {\
            (collision)->x1 = (int)tx[i];\
        }\
        if( (collision)->x2 < (int)tx[i] )\
        {\
            (collision)->x2 = (int)tx[i];\
        }\
        if( (collision)->y1 > (int)ty[i] )\
        {\
            (collision)->y1 = (int)ty[i];\
        }\
        if( (collision)->y2 < (int)ty[i] )\
        {\
            (collision)->y2 = (int)ty[i];\
        }\
    }\
}

/* ���S�_�Z�o */
#define set_center( sprite, collision )\
{\
    struct DXRubyImage *image;\
    if( sprite->vcenter_x == Qnil || sprite->vcenter_y == Qnil )\
    {\
        DXRUBY_CHECK_IMAGE( sprite->vimage );\
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );\
        collision->center_x = sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);\
        collision->center_y = sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);\
    }\
    else\
    {\
        collision->center_x = NUM2FLOAT(sprite->vcenter_x);\
        collision->center_y = NUM2FLOAT(sprite->vcenter_y);\
    }\
}

/* �_�̉�] */
#define rotation_point( collision, tx, ty, bx, by ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
    float tmpx = (bx), tmpy = (by);\
\
    tx = (tmpx - collision->center_x) * data1x - (tmpy - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty = (tmpx - collision->center_x) * data2x + (tmpy - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* �_�̉�](���S�w��E�X�P�[�����O�Ȃ�) */
#define rotation_point_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * (angle);\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx = (x), rby = (y);\
\
    x = (rbx - (centerx)) * cosa - (rby - (centery)) * sina + (centerx);\
    y = (rbx - (centerx)) * sina + (rby - (centery)) * cosa + (centery);\
}

/* ��`���ꎩ�g�̉�](�X�P�[�����O����) */
#define rotation_box( collision, tx, ty ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
\
    tx[0] = (collision->bx1 - collision->center_x) * data1x - (collision->by1 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[0] = (collision->bx1 - collision->center_x) * data2x + (collision->by1 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[1] = (collision->bx2 - collision->center_x) * data1x - (collision->by1 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[1] = (collision->bx2 - collision->center_x) * data2x + (collision->by1 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[2] = (collision->bx2 - collision->center_x) * data1x - (collision->by2 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[2] = (collision->bx2 - collision->center_x) * data2x + (collision->by2 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[3] = (collision->bx1 - collision->center_x) * data1x - (collision->by2 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[3] = (collision->bx1 - collision->center_x) * data2x + (collision->by2 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* ��`�̉�]�i���S�w��E�X�P�[�����O�Ȃ��j */
#define rotation_box_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * angle;\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx1 = x[0], rby1 = y[0], rbx2 = x[1], rby2 = y[1], rbx3 = x[2], rby3 = y[2], rbx4 = x[3], rby4 = y[3];\
\
    x[0] = (rbx1 - centerx) * cosa - (rby1 - centery) * sina + centerx;\
    y[0] = (rbx1 - centerx) * sina + (rby1 - centery) * cosa + centery;\
    x[1] = (rbx2 - centerx) * cosa - (rby2 - centery) * sina + centerx;\
    y[1] = (rbx2 - centerx) * sina + (rby2 - centery) * cosa + centery;\
    x[2] = (rbx3 - centerx) * cosa - (rby3 - centery) * sina + centerx;\
    y[2] = (rbx3 - centerx) * sina + (rby3 - centery) * cosa + centery;\
    x[3] = (rbx4 - centerx) * cosa - (rby4 - centery) * sina + centerx;\
    y[3] = (rbx4 - centerx) * sina + (rby4 - centery) * cosa + centery;\
}

/* ��`�̊g��E�k���i��]�Ȃ��j */
#define scaling_box( collision, x, y ) \
{\
    x[0] = x[3] = (collision->bx1 - collision->center_x) * collision->scale_x + collision->center_x + collision->base_x;\
    y[0] = y[1] = (collision->by1 - collision->center_y) * collision->scale_y + collision->center_y + collision->base_y;\
    x[1] = x[2] = (collision->bx2 - collision->center_x) * collision->scale_x + collision->center_x + collision->base_x;\
    y[2] = y[3] = (collision->by2 - collision->center_y) * collision->scale_y + collision->center_y + collision->base_y;\
}

/* �O�p�`���ꎩ�g�̉�](�X�P�[�����O����) */
#define rotation_triangle( collision, x, y, tx, ty ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
\
    tx[0] = (x[0] - collision->center_x) * data1x - (y[0] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[0] = (x[0] - collision->center_x) * data2x + (y[0] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[1] = (x[1] - collision->center_x) * data1x - (y[1] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[1] = (x[1] - collision->center_x) * data2x + (y[1] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[2] = (x[2] - collision->center_x) * data1x - (y[2] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[2] = (x[2] - collision->center_x) * data2x + (y[2] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* �O�p�`�̉�]�i���S�w��E�X�P�[�����O�Ȃ��j */
#define rotation_triangle_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * angle;\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx1 = x[0], rby1 = y[0], rbx2 = x[1], rby2 = y[1], rbx3 = x[2], rby3 = y[2];\
\
    x[0] = (rbx1 - centerx) * cosa - (rby1 - centery) * sina + centerx;\
    y[0] = (rbx1 - centerx) * sina + (rby1 - centery) * cosa + centery;\
    x[1] = (rbx2 - centerx) * cosa - (rby2 - centery) * sina + centerx;\
    y[1] = (rbx2 - centerx) * sina + (rby2 - centery) * cosa + centery;\
    x[2] = (rbx3 - centerx) * cosa - (rby3 - centery) * sina + centerx;\
    y[2] = (rbx3 - centerx) * sina + (rby3 - centery) * cosa + centery;\
}


#define intersect(x1, y1, x2, y2, x3, y3, x4, y4) ( ((x1 - x2) * (y3 - y1) + (y1 - y2) * (x1 - x3)) * \
                                                    ((x1 - x2) * (y4 - y1) + (y1 - y2) * (x1 - x4)) )

struct Vector {
    float x;
    float y;
};


/*--------------------------------------------------------------------
    (���������p)�O�p�Ɠ_�̔���
 ---------------------------------------------------------------------*/
/* �E���̎O�p�Ɠ_ */
static int checktriangle( float x, float y, float x1, float y1, float x2, float y2, float x3, float y3 )
{
    float cx, cy;

    if( (x1 - x3) * (y1 - y2) == (x1 - x2) * (y1 - y3) )
    {
        return 0;
    }

    cx = (x1 + x2 + x3) / 3; /* ���S�_x */
    cy = (y1 + y2 + y3) / 3; /* ���S�_y */

    if( intersect( x1, y1, x2, y2, x, y, cx, cy ) < 0.0f ||
        intersect( x2, y2, x3, y3, x, y, cx, cy ) < 0.0f ||
        intersect( x3, y3, x1, y1, x, y, cx, cy ) < 0.0f )
    {
        return 0;
    }
    return -1;
}


/*--------------------------------------------------------------------
    (���������p)�~�Ɛ����̔���
 ---------------------------------------------------------------------*/
/* �~�Ɛ����̔��� */
static int checkCircleLine( float x, float y, float r, float x1, float y1, float x2, float y2 )
{
    float n1, n2, n3;
    /* v�͐����n�_����I�_ */
    /* c�͐����n�_����~���S */
    struct Vector v = {x2 - x1, y2 - y1};
    struct Vector c = {x - x1, y - y1};

    if( v.x == 0.0f && v.y == 0.0f )
    {
        return check_circle_point(x, y, r, x1, y1);
    }

    /* ��̃x�N�g���̓��ς����߂� */
    n1 = v.x * c.x + v.y * c.y;

    if( n1 < 0 )
    {
        /* c�̒������~�̔��a��菬�����ꍇ�͌������Ă��� */
        return c.x*c.x + c.y*c.y < r * r ? -1 : 0;
    }

    n2 = v.x * v.x + v.y * v.y;

    if( n1 > n2 )
    {
        float len;
        /* �����̏I�_�Ɖ~�̒��S�̋����̓������߂� */
        len = (x2 - x)*(x2 - x) + (y2 - y)*(y2 - y);
        /* �~�̔��a�̓������������ꍇ�͌������Ă��� */
        return  len < r * r ? -1 : 0;
    }
    else
    {
        n3 = c.x * c.x + c.y * c.y;
        return ( n3-(n1/n2)*n1 < r * r ) ? -1 : 0;
    }
    return 0;
}


/* �ȉ~�\���� */
struct ELLIPSE
{
   float fRad_X; /* X���a */
   float fRad_Y; /* Y���a */
   float fAngle; /* ��]�p�x */
   float fCx; /* ����_X���W */
   float fCy; /* ����_Y���W */
};

/* �ȉ~�Փ˔���֐� */
/* http://marupeke296.com/COL_2D_No7_EllipseVsEllipse.html */
int CollisionEllipse( struct ELLIPSE E1, struct ELLIPSE E2 )
{
   /* STEP1 : E2��P�ʉ~�ɂ���ϊ���E1�Ɏ{�� */
   float DefAng = E1.fAngle-E2.fAngle;
   float Cos = cos( DefAng );
   float Sin = sin( DefAng );
   float nx = E2.fRad_X * Cos;
   float ny = -E2.fRad_X * Sin;
   float px = E2.fRad_Y * Sin;
   float py = E2.fRad_Y * Cos;
   float ox = cos( E1.fAngle )*(E2.fCx-E1.fCx) + sin(E1.fAngle)*(E2.fCy-E1.fCy);
   float oy = -sin( E1.fAngle )*(E2.fCx-E1.fCx) + cos(E1.fAngle)*(E2.fCy-E1.fCy);

   /* STEP2 : ��ʎ�A�`G�̎Z�o */
   float rx_pow2 = 1/(E1.fRad_X*E1.fRad_X);
   float ry_pow2 = 1/(E1.fRad_Y*E1.fRad_Y);
   float A = rx_pow2*nx*nx + ry_pow2*ny*ny;
   float B = rx_pow2*px*px + ry_pow2*py*py;
   float D = 2*rx_pow2*nx*px + 2*ry_pow2*ny*py;
   float E = 2*rx_pow2*nx*ox + 2*ry_pow2*ny*oy;
   float F = 2*rx_pow2*px*ox + 2*ry_pow2*py*oy;
   float G = (ox/E1.fRad_X)*(ox/E1.fRad_X) + (oy/E1.fRad_Y)*(oy/E1.fRad_Y) - 1;

   /* STEP3 : ���s�ړ���(h,k)�y�щ�]�p�x�Ƃ̎Z�o */
   float tmp1 = 1/(D*D-4*A*B);
   float h = (F*D-2*E*B)*tmp1;
   float k = (E*D-2*A*F)*tmp1;
   float Th = (B-A)==0?0:atan( D/(B-A) ) * 0.5f;

   /* STEP4 : +1�ȉ~�����ɖ߂������œ����蔻�� */
   float CosTh = cos(Th);
   float SinTh = sin(Th);
   float A_tt = A*CosTh*CosTh + B*SinTh*SinTh - D*CosTh*SinTh;
   float B_tt = A*SinTh*SinTh + B*CosTh*CosTh + D*CosTh*SinTh;
   float KK = A*h*h + B*k*k + D*h*k - E*h - F*k + G > 0 ? 0 : A*h*h + B*k*k + D*h*k - E*h - F*k + G;
   float Rx_tt = 1+sqrt(-KK/A_tt);
   float Ry_tt = 1+sqrt(-KK/B_tt);
   float x_tt = CosTh*h-SinTh*k;
   float y_tt = SinTh*h+CosTh*k;
   float JudgeValue = x_tt*x_tt/(Rx_tt*Rx_tt) + y_tt*y_tt/(Ry_tt*Ry_tt);

   if( JudgeValue <= 1 )
      return TRUE; /* �Փ� */

   return FALSE;
}


int check( struct DXRubyCollisionGroup *o, struct DXRubyCollisionGroup *d )
{
    int i, j;

    if( o->count == 1 && d->count == 1 )
    {
        struct DXRubyCollision *co = g_volume_pointer + o->index;
        struct DXRubyCollision *cd = g_volume_pointer + d->index;

        /* �ڍ׃`�F�b�N */
        if( check_sub( co, cd ) )
        {
            return TRUE;
        }
    }
    else
    {
        for( i = 0; i < o->count; i++ )
        {
            for( j = 0; j < d->count; j++ )
            {
                struct DXRubyCollision *co = g_volume_pointer + o->index + i;
                struct DXRubyCollision *cd = g_volume_pointer + d->index + j;

                /* ���E�{�����[���`�F�b�N���ڍ׃`�F�b�N */
                if( check_box_box( co, cd ) && check_sub( co, cd ) )
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}


int check_sub( struct DXRubyCollision *o, struct DXRubyCollision *d )
{
    struct DXRubySprite *o_sprite = DXRUBY_GET_STRUCT( Sprite, o->vsprite );
    struct DXRubySprite *d_sprite = DXRUBY_GET_STRUCT( Sprite, d->vsprite );
    int o_type;
    int d_type;

    /* collision�ȗ��� */
    if( o->vcollision != Qnil )
    {
        o_type = RARRAY_LEN( o->vcollision );
    }
    else
    {
        o_type = 4;
    }

    if( d->vcollision != Qnil )
    {
        d_type = RARRAY_LEN( d->vcollision );
    }
    else
    {
        d_type = 4;
    }

    if( o_type == d_type )
    {/* �����`�̔�r */
        switch ( o_type )
        {
        case 4: /* ��` */
                /* ����������ŉ�]���������� */

            if( o->rotation_flg || o->scaling_flg ) /* o������]���Ă��� */
            { /* d��o���S�ɉ�]���ċ��E�{�����[��������r����B�������Ă��Ȃ���Γ������Ă��Ȃ��B */
                float ox[4], oy[4];
                float dx[4], dy[4];
                struct DXRubyCollision o_collision, d_collision;
                float centerx, centery;

                if( d->rotation_flg || d->scaling_flg ) /* d������]���Ă��� */
                {
                    /* d���������̎p���ɉ�] */
                    rotation_box( d, dx, dy );
                }
                else /* d���͉�]���Ă��Ȃ����� */
                {
                    dx[0] = dx[3] = (float)d->x1;
                    dy[0] = dy[1] = (float)d->y1;
                    dx[1] = dx[2] = (float)d->x2;
                    dy[2] = dy[3] = (float)d->y2;
                }

                /* o�����S�_ */
                centerx = o->center_x + o->base_x;
                centery = o->center_y + o->base_y;

                /* o�����S�_�𒆐S��d����] */
                rotation_box_out( centerx, centery, -o->angle, dx, dy )

                /* d�����E�{�����[���쐬 */
                volume_box( 4, dx, dy, &d_collision );

                /* o���̃X�P�[�����O */
                scaling_box( o, ox, oy );

                /* o�����E�{�����[���쐬 */
                volume_box( 4, ox, oy, &o_collision );

                if( !check_box_box( &o_collision, &d_collision ) )
                {
                    return FALSE; /* �������Ă��Ȃ����� */
                }
            }

            if( d->rotation_flg || d->scaling_flg ) /* d������]���Ă��� */
            { /* o��d���S�ɉ�]���ċ��E�{�����[��������r����B�������Ă��Ȃ���Γ������Ă��Ȃ��B */
                float ox[4], oy[4];
                float dx[4], dy[4];
                struct DXRubyCollision o_collision, d_collision;
                float centerx, centery;

                if( o->rotation_flg || o->scaling_flg ) /* o������]���Ă��� */
                {
                    /* o���������̎p���ɉ�] */
                    rotation_box( o, ox, oy );
                }
                else /* o���͉�]���Ă��Ȃ����� */
                {
                    ox[0] = ox[3] = (float)o->x1;
                    oy[0] = oy[1] = (float)o->y1;
                    ox[1] = ox[2] = (float)o->x2;
                    oy[2] = oy[3] = (float)o->y2;
                }

                /* d�����S�_ */
                centerx = d->center_x + d->base_x;
                centery = d->center_y + d->base_y;

                /* d�����S�_�𒆐S��o����] */
                rotation_box_out( centerx, centery, -d->angle, ox, oy )

                /* o�����E�{�����[���쐬 */
                volume_box( 4, ox, oy, &o_collision );

                /* d���̃X�P�[�����O */
                scaling_box( d, dx, dy );

                /* d�����E�{�����[���쐬 */
                volume_box( 4, dx, dy, &d_collision );

                if( !check_box_box( &o_collision, &d_collision ) )
                {
                    return FALSE; /* �������Ă��Ȃ����� */
                }
            }
            return TRUE; /* �����ɗ������_�œ������Ă��� */
            break;
        case 3: /* �~���m */
        {

            if( (o->scale_x != o->scale_y && RTEST(o_sprite->vcollision_sync)) || /* o�����ȉ~ */
                (d->scale_x != d->scale_y && RTEST(d_sprite->vcollision_sync)) )  /* d�����ȉ~ */
            { /* �ǂ��������ȉ~�Ȃ�ȉ~�������ł̏Փ˔�������� */
                struct ELLIPSE e1, e2;
                float ox, oy, or, dx, dy, dr;
                if( RTEST(o_sprite->vcollision_sync) )
                {
                    rotation_point( o, ox, oy, NUM2FLOAT(RARRAY_AREF(o->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(o->vcollision, 1)) );
                }
                else
                {
                    ox = o->base_x + NUM2INT(RARRAY_AREF(o->vcollision, 0));
                    oy = o->base_y + NUM2INT(RARRAY_AREF(o->vcollision, 1));
                }
                or = NUM2FLOAT(RARRAY_AREF(o->vcollision, 2));

                e1.fCx = ox;
                e1.fCy = oy;
                if( RTEST(o_sprite->vcollision_sync) )
                {
                    e1.fRad_X = o->scale_x * or * 1;
                    e1.fRad_Y = o->scale_y * or * 1;
                    e1.fAngle = 3.141592653589793115997963468544185161590576171875f / 180.0f * o->angle;
                }
                else
                {
                    e1.fRad_X = or * 1;
                    e1.fRad_Y = or * 1;
                    e1.fAngle = 0;
                }

                if( RTEST(d_sprite->vcollision_sync) )
                {
                    rotation_point( d, dx, dy, NUM2FLOAT(RARRAY_AREF(d->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(d->vcollision, 1)) );
                }
                else
                {
                    dx = d->base_x + NUM2INT(RARRAY_AREF(d->vcollision, 0));
                    dy = d->base_y + NUM2INT(RARRAY_AREF(d->vcollision, 1));
                }
                dr = NUM2FLOAT(RARRAY_AREF(d->vcollision, 2));

                e2.fCx = dx;
                e2.fCy = dy;
                if( RTEST(d_sprite->vcollision_sync) )
                {
                    e2.fRad_X = d->scale_x * dr * 1;
                    e2.fRad_Y = d->scale_y * dr * 1;
                    e2.fAngle = 3.141592653589793115997963468544185161590576171875f / 180.0f * d->angle;
                }
                else
                {
                    e2.fRad_X = dr * 1;
                    e2.fRad_Y = dr * 1;
                    e2.fAngle = 0;
                }

                return CollisionEllipse( e1, e2 );
            }
            else
            { /* �^�~���m */
                float ox, oy, or, dx, dy, dr;

                if( o->rotation_flg ) /* o������]���Ă��� */
                {
                    rotation_point( o, ox, oy, NUM2FLOAT(RARRAY_AREF(o->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(o->vcollision, 1)) );
                }
                else
                {
                    ox = o->base_x + NUM2INT(RARRAY_AREF(o->vcollision, 0));
                    oy = o->base_y + NUM2INT(RARRAY_AREF(o->vcollision, 1));
                }
                or = NUM2FLOAT(RARRAY_AREF(o->vcollision, 2)) * o->scale_x;

                if( d->rotation_flg ) /* d������]���Ă��� */
                {
                    rotation_point( d, dx, dy, NUM2FLOAT(RARRAY_AREF(d->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(d->vcollision, 1)) );
                }
                else
                {
                    dx = d->base_x + NUM2INT(RARRAY_AREF(d->vcollision, 0));
                    dy = d->base_y + NUM2INT(RARRAY_AREF(d->vcollision, 1));
                }
                dr = NUM2FLOAT(RARRAY_AREF(d->vcollision, 2)) * d->scale_x;

                return check_circle_circle( ox, oy, or, dx, dy, dr );
            }
        }
            break;
        case 6: /* �O�p */
            {
                float ox[3], oy[3];
                float dx[3], dy[3];
                float x[3], y[3];

                if( o->rotation_flg || o->scaling_flg ) /* o������]���Ă��� */
                {
                    x[0] = NUM2INT(RARRAY_AREF(o->vcollision, 0)) + 0.5f;
                    y[0] = NUM2INT(RARRAY_AREF(o->vcollision, 1)) + 0.5f;
                    x[1] = NUM2INT(RARRAY_AREF(o->vcollision, 2)) + 0.5f;
                    y[1] = NUM2INT(RARRAY_AREF(o->vcollision, 3)) + 0.5f;
                    x[2] = NUM2INT(RARRAY_AREF(o->vcollision, 4)) + 0.5f;
                    y[2] = NUM2INT(RARRAY_AREF(o->vcollision, 5)) + 0.5f;

                    set_center( o_sprite, o );
                    rotation_triangle( o, x, y, ox, oy );
                }
                else
                {
                    ox[0] = NUM2INT(RARRAY_AREF(o->vcollision, 0)) + 0.5f + o->base_x;
                    oy[0] = NUM2INT(RARRAY_AREF(o->vcollision, 1)) + 0.5f + o->base_y;
                    ox[1] = NUM2INT(RARRAY_AREF(o->vcollision, 2)) + 0.5f + o->base_x;
                    oy[1] = NUM2INT(RARRAY_AREF(o->vcollision, 3)) + 0.5f + o->base_y;
                    ox[2] = NUM2INT(RARRAY_AREF(o->vcollision, 4)) + 0.5f + o->base_x;
                    oy[2] = NUM2INT(RARRAY_AREF(o->vcollision, 5)) + 0.5f + o->base_y;
                }

                if( d->rotation_flg || d->scaling_flg ) /* d������]���Ă��� */
                {
                    x[0] = NUM2INT(RARRAY_AREF(d->vcollision, 0)) + 0.5f;
                    y[0] = NUM2INT(RARRAY_AREF(d->vcollision, 1)) + 0.5f;
                    x[1] = NUM2INT(RARRAY_AREF(d->vcollision, 2)) + 0.5f;
                    y[1] = NUM2INT(RARRAY_AREF(d->vcollision, 3)) + 0.5f;
                    x[2] = NUM2INT(RARRAY_AREF(d->vcollision, 4)) + 0.5f;
                    y[2] = NUM2INT(RARRAY_AREF(d->vcollision, 5)) + 0.5f;

                    set_center( d_sprite, d );
                    rotation_triangle( d, x, y, dx, dy );
                }
                else
                {
                    dx[0] = NUM2INT(RARRAY_AREF(d->vcollision, 0)) + 0.5f + d->base_x;
                    dy[0] = NUM2INT(RARRAY_AREF(d->vcollision, 1)) + 0.5f + d->base_y;
                    dx[1] = NUM2INT(RARRAY_AREF(d->vcollision, 2)) + 0.5f + d->base_x;
                    dy[1] = NUM2INT(RARRAY_AREF(d->vcollision, 3)) + 0.5f + d->base_y;
                    dx[2] = NUM2INT(RARRAY_AREF(d->vcollision, 4)) + 0.5f + d->base_x;
                    dy[2] = NUM2INT(RARRAY_AREF(d->vcollision, 5)) + 0.5f + d->base_y;
                }

                return check_line_line(ox[0], oy[0], ox[1], oy[1], dx[1], dy[1], dx[2], dy[2]) ||
                       check_line_line(ox[0], oy[0], ox[1], oy[1], dx[2], dy[2], dx[0], dy[0]) ||
                       check_line_line(ox[1], oy[1], ox[2], oy[2], dx[0], dy[0], dx[1], dy[1]) ||
                       check_line_line(ox[1], oy[1], ox[2], oy[2], dx[2], dy[2], dx[0], dy[0]) ||
                       check_line_line(ox[2], oy[2], ox[0], oy[0], dx[0], dy[0], dx[1], dy[1]) ||
                       check_line_line(ox[2], oy[2], ox[0], oy[0], dx[1], dy[1], dx[2], dy[2]) ||
                       checktriangle(ox[0], oy[0], dx[0], dy[0], dx[1], dy[1], dx[2], dy[2]) ||
                       checktriangle(dx[0], dy[0], ox[0], oy[0], ox[1], oy[1], ox[2], oy[2]);
            }
            break;
        case 2: /* �_ */
            return TRUE; /* �����ɗ������_�œ������Ă��� */
            break;
        default:
            rb_raise( eDXRubyError, "Internal error" );
        }
    }
    else
    {/* �Ⴄ�`�̔�r */
        if( o_type > d_type )
        {/* o�̂ق�������������ւ� */
            struct DXRubyCollision *ctemp;
            int itemp;
            ctemp = o;
            o = d;
            d = ctemp;
            itemp = o_type;
            o_type = d_type;
            d_type = itemp;
        }

        switch( o_type )
        {
        case 2: /* �_ */
            {
                struct DXRubyCollision *point_collision = o;

                switch( d_type)
                {
                case 3: /* �_�Ɖ~ */
                    {
                        struct DXRubyCollision *circle_collision = d;
                        float cx, cy, cr;
                        float px, py;
                        float center_x, center_y;
                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        rotation_point( point_collision, px, py, point_collision->bx1 + 0.5f, point_collision->by1 + 0.5f );

                        if( circle_collision->rotation_flg ) /* �~����]���Ă��� */
                        {   /* �_���~��]���S�x�[�X�ŉ�]������ */
                            rotation_point_out( center_x, center_y, -circle_collision->angle, px, py );
                        }

                        cx = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)) + circle_collision->base_x;
                        cy = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) + circle_collision->base_y;
                        cr = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        if( circle_collision->scaling_flg ) /* �~�����ό`���Ă��� */
                        {   /* �~���^�~�ɂȂ�悤�ɓ_�̍��W��ό`������ */
                            px = (px - center_x) / circle_collision->scale_x + center_x;
                            py = (py - center_y) / circle_collision->scale_y + center_y;
                        }

                        return check_circle_point( cx, cy, cr, px, py );
                    }
                    break;
                case 4: /* �_�Ƌ�` */
                    {
                        struct DXRubyCollision *box_collision = d;

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* ��`����]���Ă��� */
                        {/* �_����`���S�ɉ�]���Ĕ�r���� */
                            float px[4], py[4];
                            float bx[4], by[4];
                            struct DXRubyCollision p_collision, b_collision;
                            float centerx, centery;

                            px[0] = px[3] = (float)point_collision->x1;
                            py[0] = py[1] = (float)point_collision->y1;
                            px[1] = px[2] = (float)point_collision->x2;
                            py[2] = py[3] = (float)point_collision->y2;

                            /* ��`���ό`���S�_ */
                            centerx = box_collision->center_x + box_collision->base_x;
                            centery = box_collision->center_y + box_collision->base_y;

                            /* ��`���ό`���S�_�𒆐S�ɓ_����] */
                            rotation_box_out( centerx, centery, -box_collision->angle, px, py )

                            /* �_�����E�{�����[���쐬 */
                            volume_box( 4, px, py, &p_collision );

                            /* ��`���̊g��E�k�� */
                            scaling_box( box_collision, bx, by );

                            /* ��`�����E�{�����[���쐬 */
                            volume_box( 4, bx, by, &b_collision );

                            if( !check_box_box( &p_collision, &b_collision ) )
                            {
                                return FALSE; /* �������Ă��Ȃ����� */
                            }
                        }
                        return TRUE; /* �����ɗ������_�œ������Ă��� */
                    }
                    break;
                case 6: /* �_�ƎO�p */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float x[3], y[3], tri_x[3], tri_y[3]; /* �O�p���W */

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* �O�p������]�E�ό`���Ă��� */
                        {
                            /* �O�p���������̎p���ɉ�]�E�ό` */
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( tri_collision, x, y, tri_x, tri_y );
                        }
                        else /* �O�p���͉�]�E�ό`���Ă��Ȃ����� */
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }
                        return checktriangle(o->x1 + 0.5f, o->y1 + 0.5f, tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
                rb_raise( eDXRubyError, "Internal error" );
            }
            break;

        case 3: /* �~ */
            {
                struct DXRubyCollision *circle_collision = o;

                switch( d_type )
                {
                case 4: /* �~�Ƌ�` */
                    {
                        struct DXRubyCollision *box_collision = d;
                        float box_x[4], box_y[4]; /* ��`���W */
                        float circle_x, circle_y, circle_r;
                        float center_x, center_y;

                        if( !circle_collision->scaling_flg || circle_collision->scale_x == circle_collision->scale_y ) /* �~���ό`���Ă��Ȃ��A�������͏c������ */
                        {
                            if( box_collision->scaling_flg ) /* ��`�����ό`���Ă��� */
                            {   /* �ό`������ */
                                scaling_box( box_collision, box_x, box_y );
                                if( box_collision->scale_x < 0 )
                                {
                                    float temp;
                                    temp = box_x[0];
                                    box_x[0] = box_x[1];
                                    box_x[1] = temp;
                                    temp = box_x[2];
                                    box_x[2] = box_x[3];
                                    box_x[3] = temp;
                                    temp = box_y[0];
                                    box_y[0] = box_y[1];
                                    box_y[1] = temp;
                                    temp = box_y[2];
                                    box_y[2] = box_y[3];
                                    box_y[3] = temp;
                                }
                                if( box_collision->scale_y < 0 )
                                {
                                    float temp;
                                    temp = box_x[0];
                                    box_x[0] = box_x[3];
                                    box_x[3] = temp;
                                    temp = box_x[2];
                                    box_x[2] = box_x[1];
                                    box_x[1] = temp;
                                    temp = box_y[0];
                                    box_y[0] = box_y[3];
                                    box_y[3] = temp;
                                    temp = box_y[2];
                                    box_y[2] = box_y[1];
                                    box_y[1] = temp;
                                }
                            }
                            else
                            {
                                box_x[0] = box_x[3] = box_collision->bx1 + box_collision->base_x;
                                box_y[0] = box_y[1] = box_collision->by1 + box_collision->base_y;
                                box_x[1] = box_x[2] = box_collision->bx2 + box_collision->base_x;
                                box_y[2] = box_y[3] = box_collision->by2 + box_collision->base_y;
                            }

                            if( circle_collision->rotation_flg || circle_collision->scaling_flg ) /* �~����]���Ă��� */
                            {
                                /* �~�������̒��S�x�[�X�ŉ�]������ */
                                rotation_point( circle_collision, circle_x, circle_y, NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) );
                            }
                            else
                            {
                                circle_x = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)) + circle_collision->base_x;
                                circle_y = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) + circle_collision->base_y;
                            }

                            if( box_collision->rotation_flg ) /* ��`������]���Ă��� */
                            {   /* �~�̒��S�_����`���S�x�[�X�ŉ�]������ */
                                rotation_point_out( box_collision->center_x + box_collision->base_x, box_collision->center_y + box_collision->base_y, -box_collision->angle, circle_x, circle_y );
                            }

                            circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2)) * circle_collision->scale_x;

                            /* ���Ƃ͉~�Ɖ�]���Ă��Ȃ���`�̔���ł�����B */
                            return check_point_box(circle_x, circle_y, box_x[0] - circle_r, box_y[0], box_x[2] + circle_r, box_y[2]) ||
                                   check_point_box(circle_x, circle_y, box_x[0], box_y[0] - circle_r, box_x[2], box_y[2] + circle_r) ||
                                   check_circle_point(box_x[0], box_y[0], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[1], box_y[1], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[2], box_y[2], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[3], box_y[3], circle_r, circle_x, circle_y);
                        }

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* ��`������]�E�ό`���Ă��� */
                        {
                            /* ��`���������̎p���ɉ�]�E�ό` */
                            rotation_box( box_collision, box_x, box_y );
                        }
                        else /* ��`���͉�]�E�ό`���Ă��Ȃ����� */
                        {
                            box_x[0] = box_x[3] = (float)box_collision->x1;
                            box_y[0] = box_y[1] = (float)box_collision->y1;
                            box_x[1] = box_x[2] = (float)box_collision->x2;
                            box_y[2] = box_y[3] = (float)box_collision->y2;
                        }

                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        if( circle_collision->rotation_flg ) /* �~����]���Ă��� */
                        {
                            /* �~���S�_�𒆐S�ɋ�`��] */
                            rotation_box_out( center_x, center_y, -circle_collision->angle, box_x, box_y )
                        }
                        circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        circle_x = circle_collision->base_x + NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0));
                        circle_y = circle_collision->base_y + NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1));
                        if( circle_collision->scaling_flg ) /* �~�����ό`���Ă��� */
                        {   /* �~���^�~�ɂȂ�悤�ɋ�`��ό`������ */
                            box_x[0] = (box_x[0] - center_x) / circle_collision->scale_x + center_x;
                            box_y[0] = (box_y[0] - center_y) / circle_collision->scale_y + center_y;
                            box_x[1] = (box_x[1] - center_x) / circle_collision->scale_x + center_x;
                            box_y[1] = (box_y[1] - center_y) / circle_collision->scale_y + center_y;
                            box_x[2] = (box_x[2] - center_x) / circle_collision->scale_x + center_x;
                            box_y[2] = (box_y[2] - center_y) / circle_collision->scale_y + center_y;
                            box_x[3] = (box_x[3] - center_x) / circle_collision->scale_x + center_x;
                            box_y[3] = (box_y[3] - center_y) / circle_collision->scale_y + center_y;
                        }

                        /* ���� */
                        return checktriangle(circle_x, circle_y, box_x[0], box_y[0], box_x[1], box_y[1], box_x[2], box_y[2]) || 
                               checktriangle(circle_x, circle_y, box_x[0], box_y[0], box_x[2], box_y[2], box_x[3], box_y[3]) || 
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[0], box_y[0], box_x[1], box_y[1]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[1], box_y[1], box_x[2], box_y[2]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[2], box_y[2], box_x[3], box_y[3]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[3], box_y[3], box_x[0], box_y[0]);
                    }
                    break;
                case 6: /* �~�ƎO�p */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float x[3], y[3], tri_x[3], tri_y[3]; /* �O�p���W */
                        float circle_x, circle_y, circle_r;
                        float center_x, center_y;

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* �O�p������]�E�ό`���Ă��� */
                        {
                            /* �O�p���������̎p���ɉ�]�E�ό` */
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( tri_collision, x, y, tri_x, tri_y );
                        }
                        else /* �O�p���͉�]�E�ό`���Ă��Ȃ����� */
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }

                        /* �~�̉�]���S */
                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        if( circle_collision->rotation_flg ) /* �~����]���Ă��� */
                        {
                            /* �~���S�_�𒆐S�ɎO�p��] */
                            rotation_triangle_out( center_x, center_y, -circle_collision->angle, tri_x, tri_y )
                        }
                        circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        circle_x = circle_collision->base_x + NUM2INT(RARRAY_AREF(circle_collision->vcollision, 0));
                        circle_y = circle_collision->base_y + NUM2INT(RARRAY_AREF(circle_collision->vcollision, 1));
                        if( circle_collision->scaling_flg ) /* �~�����ό`���Ă��� */
                        {   /* �~���^�~�ɂȂ�悤�ɎO�p��ό`������ */
                            tri_x[0] = (tri_x[0] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[0] = (tri_y[0] - center_y) / circle_collision->scale_y + center_y;
                            tri_x[1] = (tri_x[1] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[1] = (tri_y[1] - center_y) / circle_collision->scale_y + center_y;
                            tri_x[2] = (tri_x[2] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[2] = (tri_y[2] - center_y) / circle_collision->scale_y + center_y;
                        }

                        /* ���� */
                        return checktriangle(circle_x, circle_y, tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) || 
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[2], tri_y[2], tri_x[0], tri_y[0]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
            }
        case 4: /* ��` */
            {
                struct DXRubyCollision *box_collision = o;

                switch( d_type )
                {
                case 6: /* ��`�ƎO�p */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float box_x[4], box_y[4]; /* ��`���W */
                        float tri_x[3], tri_y[3]; /* �O�p���W */
                        float x[3], y[3];

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* ��`����]���Ă��� */
                        {
                            rotation_box( box_collision, box_x, box_y );
                        }
                        else
                        {
                            box_x[0] = box_x[3] = (float)box_collision->x1;
                            box_y[0] = box_y[1] = (float)box_collision->y1;
                            box_x[1] = box_x[2] = (float)box_collision->x2;
                            box_y[2] = box_y[3] = (float)box_collision->y2;
                        }

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* �O�p����]���Ă��� */
                        {
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( d, x, y, tri_x, tri_y );
                        }
                        else
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }

                        return check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               checktriangle(tri_x[0], tri_y[0], box_x[0], box_y[0], box_x[1], box_y[1], box_x[2], box_y[2]) || 
                               checktriangle(tri_x[0], tri_y[0], box_x[0], box_y[0], box_x[2], box_y[2], box_x[3], box_y[3]) || 
                               checktriangle(box_x[0], box_y[0], tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
            }
        default:
            rb_raise( eDXRubyError, "Internal error" );
            break;
        }
        rb_raise( eDXRubyError, "Internal error" );
    }
    rb_raise( eDXRubyError, "Internal error" );
}

/* �Փ˔���z��̃J�E���g�擾 */
int get_volume_count( VALUE vary )
{
    int p, count = 0;

    for( p = 0; p < RARRAY_LEN(vary); p++ )
    {
        VALUE vsprite = RARRAY_AREF( vary, p );

        if( TYPE(vsprite) == T_ARRAY )
        {
            count += get_volume_count( vsprite );
        }
        else
        {
            count++;
        }
    }

    return count;
}


/* AABB���E�{�����[�����̖��ח̈� */
static int alloc_volume( int count )
{
    while( g_volume_allocate_count < g_volume_count + count )
    {
        g_volume_allocate_count = g_volume_allocate_count * 3 / 2; /* 1.5�{�ɂ��� */
        g_volume_pointer = realloc( g_volume_pointer, sizeof( struct DXRubyCollision ) * g_volume_allocate_count );
    }

    g_volume_count += count;

    return g_volume_count - count;
}

/* �z��̏Փ˔���pAABB���E�{�����[���쐬�B�̈�͏�̊֐��Ŋm�ۂ��ēn����� */
int make_volume_ary( VALUE vary, struct DXRubyCollisionGroup *group )
{
    int p, count = 0;

    /* �z��̐������{�����[���쐬���� */
    for( p = 0; p < RARRAY_LEN(vary); p++ )
    {
        int tmp;
        VALUE vsprite = RARRAY_AREF( vary, p );

        if( TYPE(vsprite) == T_ARRAY )
        {
            tmp = make_volume_ary( vsprite, group );
        }
        else
        {
            tmp = make_volume( vsprite, group );
        }
        count += tmp;
        group += tmp;
    }

    return count;
}


/* �P�̂̏Փ˔���pAABB���E�{�����[���쐬 */
int make_volume( VALUE vsprite, struct DXRubyCollisionGroup *group )
{
    struct DXRubySprite *sprite;

    /* Sprite����Ȃ���Ζ��� */
    if( !DXRUBY_CHECK( Sprite, vsprite ) )
    {
        return 0;
    }

    /* �Փ˔��肪�L���łȂ��ꍇ�͖��� */
    sprite = DXRUBY_GET_STRUCT( Sprite, vsprite );
#ifdef DXRUBY15
    if( !RTEST(sprite->vvisible) || !RTEST(sprite->vcollision_enable) || sprite->vanish )
#else
    if( !RTEST(sprite->vcollision_enable) || sprite->vanish )
#endif
    {
        return 0;
    }

    /* �Փ˔���͈͂��ݒ肳��Ă��Ȃ����1�������׏��𐶐����� */
    if( sprite->vcollision == Qnil )
    {
        /* collision��image���ݒ肳��Ă��Ȃ���Ζ��� */
        if( sprite->vimage == Qnil )
        {
            return 0;
        }

        group->vsprite = vsprite;
        group->index = alloc_volume( 1 );
        group->count = 1;
        make_volume_sub( vsprite, sprite->vcollision, g_volume_pointer + group->index );
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;

        return 1;
    }

    /* �Փ˔���͈͂��z�񂶂�Ȃ���Ζ��� */
    Check_Type( sprite->vcollision, T_ARRAY );
    if( RARRAY_LEN(sprite->vcollision) == 0 )
    {
        return 0;
    }

    /* �Փ˔���͈͔z��̒��ɔz�񂪓����Ă����ꍇ�͕����͈̔͂�p���邱�Ƃ��ł��� */
    if( TYPE(RARRAY_AREF(sprite->vcollision, 0)) == T_ARRAY )
    {
        int p2;

        group->vsprite = vsprite;
        group->index = alloc_volume( RARRAY_LEN(sprite->vcollision) );
        group->count = RARRAY_LEN(sprite->vcollision);

        for( p2 = 0; p2 < RARRAY_LEN(sprite->vcollision); p2++ )
        {
            Check_Type( RARRAY_AREF(sprite->vcollision, p2), T_ARRAY );
            make_volume_sub( vsprite, RARRAY_AREF(sprite->vcollision, p2), g_volume_pointer + group->index + p2 );
        }

        /* �O���[�v��AABB���E�����ׂĊ܂ލŏ���AABB���E�𐶐����� */
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;
        for( p2 = 1; p2 < RARRAY_LEN(sprite->vcollision); p2++ )
        {
            if( group->x1 > (g_volume_pointer + group->index + p2)->x1 )
            {
                group->x1 = (g_volume_pointer + group->index + p2)->x1;
            }
            if( group->x2 < (g_volume_pointer + group->index + p2)->x2 )
            {
                group->x2 = (g_volume_pointer + group->index + p2)->x2;
            }
            if( group->y1 > (g_volume_pointer + group->index + p2)->y1 )
            {
                group->y1 = (g_volume_pointer + group->index + p2)->y1;
            }
            if( group->y2 < (g_volume_pointer + group->index + p2)->y2 )
            {
                group->y2 = (g_volume_pointer + group->index + p2)->y2;
            }
        }
        
    }
    else
    { /* �z�񂶂�Ȃ��ꍇ */
        group->vsprite = vsprite;
        group->index = alloc_volume( 1 );
        group->count = 1;
        make_volume_sub( vsprite, sprite->vcollision, g_volume_pointer + group->index );
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;
    }

    return 1;
}


/* �Փ˔���pAABB���E�{�����[���쐬sub */
void make_volume_sub( VALUE vsprite, VALUE vcol, struct DXRubyCollision *collision )
{
    struct DXRubySprite *sprite;
    sprite = DXRUBY_GET_STRUCT( Sprite, vsprite );

    collision->vsprite = vsprite;
    collision->base_x = NUM2FLOAT(sprite->vx);
    collision->base_y = NUM2FLOAT(sprite->vy);
    if( RTEST(sprite->voffset_sync) )
    {
        struct DXRubyImage *image;
        if( sprite->vcenter_x == Qnil || sprite->vcenter_y == Qnil )
        {
            DXRUBY_CHECK_IMAGE( sprite->vimage );
            image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
            collision->base_x -= sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);
            collision->base_y -= sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);
        }
        else
        {
            collision->base_x -= NUM2FLOAT(sprite->vcenter_x);
            collision->base_y -= NUM2FLOAT(sprite->vcenter_y);
        }
    }
    collision->angle = NUM2FLOAT(sprite->vangle);
    collision->scale_x = NUM2FLOAT(sprite->vscale_x);
    collision->scale_y = NUM2FLOAT(sprite->vscale_y);
    collision->vcollision = vcol;
    if( RTEST(sprite->vcollision_sync) ) /* ��]�E�X�P�[�����O�̃t���O */
    {
        if( collision->angle != 0.0f )
        {
            collision->rotation_flg = TRUE;
        }
        else
        {
            collision->rotation_flg = FALSE;
        }
        if( collision->scale_x != 1.0f || collision->scale_y != 1.0f )
        {
            collision->scaling_flg = TRUE;
        }
        else
        {
            collision->scaling_flg = FALSE;
        }
    }
    else
    {
        collision->rotation_flg = FALSE;
        collision->scaling_flg = FALSE;
    }

    if( vcol != Qnil )
    {
        Check_Type( vcol, T_ARRAY );

        switch (RARRAY_LEN(vcol))
        {
        case 2: /* �_ */
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 0)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 1)) + 1;
                collision->x1 = (int)(collision->base_x + collision->bx1);
                collision->y1 = (int)(collision->base_y + collision->by1);
                collision->x2 = (int)(collision->base_x + collision->bx2);
                collision->y2 = (int)(collision->base_y + collision->by2);
            }
            else /* ��]�����_ */
            {
                float tx,ty;
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 0)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 1)) + 1;

                set_center( sprite, collision );
                rotation_point( collision, tx, ty, collision->bx1 + 0.5f, collision->by1 + 0.5f );

                collision->x1 = (int)tx;
                collision->y1 = (int)ty;
                collision->x2 = (int)tx + 1;
                collision->y2 = (int)ty + 1;
            }
            break;
        case 3: /* �~ */
        {
            float tempx = NUM2FLOAT(RARRAY_AREF(vcol, 0));
            float tempy = NUM2FLOAT(RARRAY_AREF(vcol, 1));
            float tempr = NUM2FLOAT(RARRAY_AREF(vcol, 2));

            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->x1 = (int)(collision->base_x + tempx - tempr);
                collision->y1 = (int)(collision->base_y + tempy - tempr);
                collision->x2 = (int)(collision->base_x + tempx + tempr);
                collision->y2 = (int)(collision->base_y + tempy + tempr);
            }
            else /* ��]�����~ */
            {
                float tx,ty;

                if( collision->scale_x != collision->scale_y ) /* �ȉ~�B����̂Ŏb��Ƃ��ċ��E�{�����[������]���ċ��E�{�����[������� */
                {
                    float x[4], y[4];
                    collision->bx1 = tempx - tempr;
                    collision->by1 = tempy - tempr;
                    collision->bx2 = tempx + tempr;
                    collision->by2 = tempy + tempr;

                    set_center( sprite, collision );

                    rotation_box( collision, x, y );
                    volume_box( 4, x, y, collision );
                    collision->x2++;
                    collision->y2++;
                }
                else /* �^�~ */
                {
                    set_center( sprite, collision );
                    rotation_point( collision, tx, ty, tempx, tempy );
                    collision->x1 = (int)(tx - tempr * collision->scale_x);
                    collision->y1 = (int)(ty - tempr * collision->scale_x);
                    collision->x2 = (int)(tx + tempr * collision->scale_x);
                    collision->y2 = (int)(ty + tempr * collision->scale_x);
                }
            }
            break;
        }
        case 4: /* ��` */
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 2)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 3)) + 1;
                collision->x1 = (int)(collision->base_x + collision->bx1);
                collision->y1 = (int)(collision->base_y + collision->by1);
                collision->x2 = (int)(collision->base_x + collision->bx2);
                collision->y2 = (int)(collision->base_y + collision->by2);
            }
            else /* ��]������` */
            {
                float tx[4], ty[4];

                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 2)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 3)) + 1;
                set_center( sprite, collision );

                rotation_box( collision, tx, ty );

                volume_box( 4, tx, ty, collision );
                collision->x2++;
                collision->y2++;
            }
            break;
        case 6: /* �O�p�` */
        {
            float tx[3], ty[3];
            int i;
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                tx[0] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 0)) + 0.5f;
                ty[0] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 1)) + 0.5f;
                tx[1] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 2)) + 0.5f;
                ty[1] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 3)) + 0.5f;
                tx[2] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 4)) + 0.5f;
                ty[2] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 5)) + 0.5f;
            }
            else /* ��]�����O�p�` */
            {
                float x[3], y[3];

                x[0] = NUM2INT(RARRAY_AREF(vcol, 0)) + 0.5f;
                y[0] = NUM2INT(RARRAY_AREF(vcol, 1)) + 0.5f;
                x[1] = NUM2INT(RARRAY_AREF(vcol, 2)) + 0.5f;
                y[1] = NUM2INT(RARRAY_AREF(vcol, 3)) + 0.5f;
                x[2] = NUM2INT(RARRAY_AREF(vcol, 4)) + 0.5f;
                y[2] = NUM2INT(RARRAY_AREF(vcol, 5)) + 0.5f;

                set_center( sprite, collision );

                rotation_triangle( collision, x, y, tx, ty );
            }
            volume_box( 3, tx, ty, collision );
            collision->x2++;
            collision->y2++;
        }
            break;
        default:
            rb_raise( eDXRubyError, "collision�̐ݒ肪�s���ł� - Sprite_make_volume" );
            break;
        }
    }
    else /* �Փ˔���͈͏ȗ����͉摜�T�C�Y�̋�`�Ƃ݂Ȃ� */
    { /* ��]���Ă��Ȃ���` */
        struct DXRubyImage *image;
        DXRUBY_CHECK_IMAGE( sprite->vimage );
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
        if( !collision->rotation_flg && !collision->scaling_flg )
        {
            collision->bx1 = 0;
            collision->by1 = 0;
            collision->bx2 = (float)image->width;
            collision->by2 = (float)image->height;
            collision->x1 = (int)collision->base_x;
            collision->y1 = (int)collision->base_y;
            collision->x2 = (int)(collision->base_x + image->width);
            collision->y2 = (int)(collision->base_y + image->height);
        }
        else /* ��]������` */
        {
            float tx[4], ty[4];

            collision->bx1 = 0;
            collision->by1 = 0;
            collision->bx2 = (float)image->width;
            collision->by2 = (float)image->height;
            collision->center_x = sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);
            collision->center_y = sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);

            rotation_box( collision, tx, ty );

            volume_box( 4, tx, ty, collision );
            collision->x2++;
            collision->y2++;
        }
    }
}

void collision_init(void)
{
    g_volume_count = 0;
    g_volume_allocate_count = 16;
    g_volume_pointer = malloc( sizeof(struct DXRubyCollision) * 16 );
}

void collision_clear(void)
{
    g_volume_count = 0;
}

