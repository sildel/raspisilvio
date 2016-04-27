uniform sampler2D tex;
varying vec2 texcoord;
uniform vec2 tex_unit;

void main(void)
{
    float x = texcoord.x;
    float y = texcoord.y;
    float x1 = x - tex_unit.x;
    float y1 = y - tex_unit.y;
    float x2 = x + tex_unit.x;
    float y2 = y + tex_unit.y;
    vec4 p0 = texture2D(tex, vec2(x1, y1));
    vec4 p1 = texture2D(tex, vec2(x, y1));
    vec4 p2 = texture2D(tex, vec2(x2, y1));
    vec4 p3 = texture2D(tex, vec2(x1, y));
    /* vec4 p4 = texture2D(tex, vec2(x, y)); */
    vec4 p5 = texture2D(tex, vec2(x2, y));
    vec4 p6 = texture2D(tex, vec2(x1, y2));
    vec4 p7 = texture2D(tex, vec2(x, y2));
    vec4 p8 = texture2D(tex, vec2(x2, y2));

    vec4 v =  p0 + (2.0 * p1) + p2 -p6 + (-2.0 * p7) + -p8;
    vec4 h =  -p0 + (-2.0 * p3) - p6 +p2 + (2.0 * p5) + p8;
    if(h.r>0.)
    {
        gl_FragColor = h;
    }
    else
    {
        gl_FragColor = vec4(-h.r,-h.g,-h.b, -h.a);
    }

    gl_FragColor.a = 1.0;
}
