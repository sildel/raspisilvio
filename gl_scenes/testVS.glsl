attribute vec2 vertex;
uniform sampler2D tex;
void main(void)
{
    vec4 col = texture2D(tex, vertex);
    gl_PointSize = 1.0;
    if(col.b==1.0 && col.b==1.0 && col.b==1.0)
    {
        gl_Position = vec4(-1.0,0.0,0.0,1.0);
    }
    else
    {
        gl_Position = vec4(0.0,0.0,0.0,1.0);
    }
}
