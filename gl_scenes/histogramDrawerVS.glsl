attribute vec2 vertex;
uniform sampler2D tex;
uniform float bins;
void main(void)
{
    vec4 col = texture2D(tex, vec2(vertex.x/bins, 0.5));
    float y;
    float x = vertex.x/bins * 2.0 -1.0;
    if(vertex.y == 1.0)
    {
        y = col.r * 2.0 - 1.0;
    }
    else
    {
        y = -1.0;
    }

    gl_Position = vec4(x, y, 0.0, 1.0);
}
