varying vec2 texcoord;
uniform sampler2D tex;
uniform sampler2D hist;
uniform ivec2 threshold;
uniform ivec2 binW;

void main(void)
{
    vec4 color = texture2D(tex, texcoord);

    float f_h = color.r * 255.0;
    float f_s = color.g * 255.0;
    float f_i = color.b * 255.0;

    int intensity = int(f_i);
    int saturation = int(f_s);
    int hue = int(f_h);

    float f_index_h = f_h/float(binW.x);

    int h_index = int(f_index_h);

    vec4 value = texture2D(hist, vec2(float(h_index)/256.0,0.0));

    int b1= int(value.r*255.0) - 9;

    int b2= int(value.g*255.0) - 9;
    
    int b3= int(value.b*255.0) - 9;
    
    int b4= int(value.a*255.0) - 9;
    
    int H_sum1 = b4*100*100*100 + b3*100*100 + b2*100 + b1;

    vec4 value2 = texture2D(hist, vec2(float(h_index+1)/256.0,0.0));

    int b5= int(value2.r*255.0) - 9;

    int b6= int(value2.g*255.0) - 9;
    
    int b7= int(value2.b*255.0) - 9;
    
    int b8= int(value2.a*255.0) - 9;
    
    int H_sum2 = b8*100*100*100 + b7*100*100 + b6*100 + b5;

    vec4 value3 = texture2D(hist, vec2(float(h_index-1)/256.0,0.0));

    int b9= int(value3.r*255.0) - 9;

    int b10= int(value3.g*255.0) - 9;
    
    int b11= int(value3.b*255.0) - 9;
    
    int b12= int(value3.a*255.0) - 9;
    
    int H_sum3 = b12*100*100*100 + b11*100*100 + b10*100 + b9;

    int h_filtered = (H_sum1 + H_sum2 + H_sum3) / 3;

    float f_index_i = f_i/float(binW.y);

    int i_index = int(f_index_i);

    vec4 value4 = texture2D(hist, vec2(float(i_index)/256.0,0.5));

    int b13= int(value4.r*255.0) - 9;

    int b14= int(value4.g*255.0) - 9;
    
    int b15= int(value4.b*255.0) - 9;
    
    int b16= int(value4.a*255.0) - 9;
    
    int I_sum1 = b16*100*100*100 + b15*100*100 + b14*100 + b13;

    vec4 value5 = texture2D(hist, vec2(float(i_index+1)/256.0,0.5));

    int b17= int(value5.r*255.0) - 9;

    int b18= int(value5.g*255.0) - 9;
    
    int b19= int(value5.b*255.0) - 9;
    
    int b20= int(value5.a*255.0) - 9;
    
    int I_sum2 = b20*100*100*100 + b19*100*100 + b18*100 + b17;

    vec4 value6 = texture2D(hist, vec2(float(i_index-1)/256.0,0.5));

    int b21= int(value6.r*255.0) - 9;

    int b22= int(value6.g*255.0) - 9;
    
    int b23= int(value6.b*255.0) - 9;
    
    int b24= int(value6.a*255.0) - 9;
    
    int I_sum3 = b24*100*100*100 + b23*100*100 + b22*100 + b21;

    int i_filtered = (I_sum1 + I_sum2 + I_sum3) / 3;

    if ( intensity < binW.y )
    {
        if ( i_filtered < threshold.y )
        {
            gl_FragColor = vec4(1);
        }
        else
        {
            gl_FragColor = vec4(0);
        }
    }
    else if ( h_filtered < threshold.x || i_filtered < threshold.y )
    {
        gl_FragColor = vec4(1);
    }
    else
    {
        gl_FragColor = vec4(0);
    }
}
