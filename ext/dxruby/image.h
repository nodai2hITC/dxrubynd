/* Image�I�u�W�F�N�g�̒��g */
struct DXRubyImage {
    struct DXRubyTexture *texture;
    int x;     /* x�n�_�ʒu      */
    int y;     /* y�n�_�ʒu      */
    int width; /* �C���[�W�̕�   */
    int height;/* �C���[�W�̍��� */
//    int lockcount;    /* ���b�N�J�E���g */
};

void Init_dxruby_Image( void );
void finalize_dxruby_Image( void );

void Image_release( struct DXRubyImage* image );
VALUE Image_drawFontEx( int argc, VALUE *argv, VALUE obj );
VALUE Image_dispose( VALUE self );
VALUE Image_allocate( VALUE klass );
VALUE Image_initialize( int argc, VALUE *argv, VALUE obj );
int array2color( VALUE color );
VALUE Image_save( int argc, VALUE *argv, VALUE self );
VALUE Image_sliceToArray( int argc, VALUE *argv, VALUE self );

