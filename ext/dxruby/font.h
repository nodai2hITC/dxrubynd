/* �t�H���g�f�[�^ */
struct DXRubyFont {
    LPD3DXFONT pD3DXFont;       /* �t�H���g�I�u�W�F�N�g   */
    HFONT hFont;                /* Image�`��Ɏg���t�H���g  */
    int size;                   /* �t�H���g�T�C�Y */
    VALUE vfontname;            /* �t�H���g���� */
    VALUE vweight;              /* ���� */
    VALUE vitalic;              /* �C�^���b�N�t���O */
    VALUE vglyph_naa;           /* �O���t�L���b�V��(AA�Ȃ�) */
    VALUE vglyph_aa;            /* �O���t�L���b�V��(AA����) */
    VALUE vauto_fitting;        /* ���ۂ̕`�敶�����w��T�C�Y�ɂȂ�悤�g�傷�� */
};

void Init_dxruby_Font( void );
void Font_release( struct DXRubyFont* font );
VALUE Font_getWidth( VALUE obj, VALUE vstr );
VALUE Font_getSize( VALUE obj );
char *Font_getGlyph( VALUE obj, UINT widechr, HDC hDC, GLYPHMETRICS *gm, VALUE vaa_flag );
void Font_getInfo_internal( VALUE vstr, struct DXRubyFont *font, int *intBlackBoxX, int *intBlackBoxY, int *intCellIncX, int *intPtGlyphOriginX, int *intPtGlyphOriginY, int *intTmAscent, int *intTmDescent );

