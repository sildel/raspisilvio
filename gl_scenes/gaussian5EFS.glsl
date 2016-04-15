#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
varying vec2 texcoord;
uniform vec2 tex_unit;

void main(void) 
{
    vec4 col = vec4(0);
    
    float x = texcoord.x;
    float y = texcoord.y;
    float xm1 = x - tex_unit.x;
    float ym1 = y - tex_unit.y;
    float xm2 = x - 2.0*tex_unit.x;
    float ym2 = y - 2.0*tex_unit.y;
    float xp1 = x + tex_unit.x;
    float yp1 = y + tex_unit.y;
    float xp2 = x + 2.0*tex_unit.x;
    float yp2 = y + 2.0*tex_unit.y;

    col += 1.0/256.0 * texture2D(tex, vec2(xm2, ym2));
    col += 4.0/256.0 * texture2D(tex, vec2(xm1, ym2));
    col += 6.0/256.0 * texture2D(tex, vec2(x, ym2));
    col += 4.0/256.0 * texture2D(tex, vec2(xp1, ym2));
    col += 1.0/256.0 * texture2D(tex, vec2(xp2, ym2));

    col += 4.0/256.0 * texture2D(tex, vec2(xm2, ym1));
    col += 16.0/256.0 * texture2D(tex, vec2(xm1, ym1));
    col += 24.0/256.0 * texture2D(tex, vec2(x, ym1));
    col += 16.0/256.0 * texture2D(tex, vec2(xp1, ym1));
    col += 4.0/256.0 * texture2D(tex, vec2(xp2, ym1));

    col += 6.0/256.0 * texture2D(tex, vec2(xm2, y));
    col += 24.0/256.0 * texture2D(tex, vec2(xm1, y));
    col += 36.0/256.0 * texture2D(tex, vec2(x, y));
    col += 24.0/256.0 * texture2D(tex, vec2(xp1, y));
    col += 6.0/256.0 * texture2D(tex, vec2(xp2, y));

    col += 4.0/256.0 * texture2D(tex, vec2(xm2, yp1));
    col += 16.0/256.0 * texture2D(tex, vec2(xm1, yp1));
    col += 24.0/256.0 * texture2D(tex, vec2(x, yp1));
    col += 16.0/256.0 * texture2D(tex, vec2(xp1, yp1));
    col += 4.0/256.0 * texture2D(tex, vec2(xp2, yp1));

    col += 1.0/256.0 * texture2D(tex, vec2(xm2, yp2));
    col += 4.0/256.0 * texture2D(tex, vec2(xm1, yp2));
    col += 6.0/256.0 * texture2D(tex, vec2(x, yp2));
    col += 4.0/256.0 * texture2D(tex, vec2(xp1, yp2));
    col += 1.0/256.0 * texture2D(tex, vec2(xp2, yp2));

    gl_FragColor = col;
}
