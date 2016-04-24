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
    color = color / 128. * 255.;
    gl_PointSize = 1.0;
    gl_Position = vec4(-1. + color,0.0,0.0,1.0);
}
