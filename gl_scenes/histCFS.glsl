varying vec2 texcoord;
uniform sampler2D tex;
uniform sampler2D hist_h;
uniform sampler2D hist_i;
uniform ivec2 threshold;

void main(void)
{
    vec4 color = texture2D(tex, texcoord);

    vec4 h_val = texture2D(hist_h,vec2(color.r,0.0));
    vec4 i_val = texture2D(hist_i,vec2(color.b,0.0));

    int b1= int(h_val.r*255.0) - 9;

    int b2= int(h_val.g*255.0) - 9;
    
    int b3= int(h_val.b*255.0) - 9;
    
    int b4= int(h_val.a*255.0) - 9;
    
    int H_sum = b4*100*100*100 + b3*100*100 + b2*100 + b1;

    int b5= int(i_val.r*255.0) - 9;

    int b6= int(i_val.g*255.0) - 9;
    
    int b7= int(i_val.b*255.0) - 9;
    
    int b8= int(i_val.a*255.0) - 9;
    
    int I_sum = b8*100*100*100 + b7*100*100 + b6*100 + b5;

    /* add a uniform for intensity umbral*/
    if ( int(color.b*255.0) < 10 )
    {
        if ( I_sum < threshold.y )
        {
            gl_FragColor = vec4(1);
        }
        else
        {
            gl_FragColor = vec4(0);
        }
    }
    else if ( H_sum < threshold.x || I_sum < threshold.y )
    {
        gl_FragColor = vec4(1);
    }
    else
    {
        gl_FragColor = vec4(0);
    }
}
