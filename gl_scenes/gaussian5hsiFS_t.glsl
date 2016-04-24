uniform sampler2D tex;
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

    float R = color.r * 255.;
    float G = color.g * 255.;
    float B = color.b * 255.;

    float H;
    float V = max(max(R, G), B);
    float S;

    if (V == 0.)
    {
        S = 0.;
    }
    else
    {
        S = (V-min(min(R, G), B))* 255. / V;
    }

    if (int(V) == int(R))
    {
        H = (G-B)*60./S;
    }
    else if (int(V) == int(G))
    {
        H = 180. + (B-R)*60./S;
    }
    else
    {
        H = 240. + (R-G)*60./S;
    }

    if (H < 0.)
    {
        H+=360.;
    }

    gl_FragColor = vec4(H/512., S/255., V/255., 1.).bgra;
}
