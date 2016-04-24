attribute vec2 vertex;

void main(void)
{
    gl_PointSize = 1.0;
    vec2 realvertex= vertex * 2.0 - 1.0;
    gl_Position = vec4(realvertex, 0.0, 1.0);
}
