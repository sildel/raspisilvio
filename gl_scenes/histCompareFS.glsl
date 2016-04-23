varying vec2 texcoord;
uniform sampler2D tex;
uniform sampler2D hist_h;
uniform sampler2D hist_i;
uniform vec2 threshold;

void main(void)
{
    vec4 color = texture2D(tex, texcoord);

    vec4 h_val = texture2D(hist_h,vec2(color.r,0.0));
    vec4 i_val = texture2D(hist_i,vec2(color.r,0.0));

    if(h_val.r < threshold.x ||  i_val.r < threshold.y)
    {
    i_val.r = 0.0;
    }
    if(h_val.r == 0.0)
    {
        gl_FragColor = vec4(1);
    }
    else
    {
        gl_FragColor = vec4(0);
    }

}
