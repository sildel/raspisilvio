uniform sampler2D tex;
varying vec2 texcoord;
uniform vec4 heads;

void main(void) 
{
    float x = texcoord.x;
    float y = texcoord.y;

    vec4 heads2 = 0.5 * (heads + 1.0);

    float m_1 = 1.0/(heads2.y-heads2.x);
    float y1_temp = m_1*(x-heads2.x);

    float m_2 = 1.0/(heads2.w-heads2.z);
    float y2_temp = m_2*(heads2.w-x);    

    if(x<heads2.x)
    {
        gl_FragColor = vec4(0);
    }
    else if((x>=heads2.x) &&(x<=heads2.y))
    {
        if(y1_temp>y)
        {
            gl_FragColor = texture2D(tex, texcoord);
            gl_FragColor.a = 1.0;
        }
        else
        {
            gl_FragColor = vec4(0);
        }
    }
    else if((x>=heads2.y) &&(x<=heads2.z))
    {
        gl_FragColor = texture2D(tex, texcoord);
        gl_FragColor.a = 1.0;
    }
    else if((x>=heads2.z) &&(x<=heads2.w))
    {
        if(y2_temp>y)
        {
            gl_FragColor = texture2D(tex, texcoord);
            gl_FragColor.a = 1.0;
        }
        else
        {
            gl_FragColor = vec4(0);
        }
    }
    else
    {
        gl_FragColor = vec4(0);
    }    
}
