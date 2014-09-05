varying vec2 texcoord;
uniform sampler2D tex;

void main(void)
{
    vec4 color = texture2D(tex, vec2(255.0/256.0,0.5));


    /*vec4 color = texture2D(tex, texcoord);*/

    int b1= int(color.r*255.0) - 9;

    int b2= int(color.g*255.0) - 9;
    
    int b3= int(color.b*255.0) - 9;
    
    int b4= int(color.a*255.0) - 9;
    
    int sum = b4*100*100*100 + b3*100*100 + b2*100 + b1;

    if (sum == 1000512)
    {
        gl_FragColor = vec4(0,0,1,1);
    }
    else
    {
        gl_FragColor = vec4(1,1,0,1);
    }
}
