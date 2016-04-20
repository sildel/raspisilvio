attribute vec2 vertex;
uniform sampler2D tex;
uniform int channel;
void main(void)
{
    vec4 col = texture2D(tex, vertex);
    float color;
    if(channel==0)
    {
        color = col.r;
    }
    else
    {
        if(channel==1)
        {
            color = col.g;
        }
        else
        {
            color = col.b;
        }
    }
    gl_PointSize = 1.0;
    float nindex = 2.0 * color - 1.0; // normalized device space
    gl_Position = vec4(nindex,0.0,0.0,1.0);
}
