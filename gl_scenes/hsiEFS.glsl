#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
varying vec2 texcoord;
uniform vec2 tex_unit;

void main(void) 
{
    float x = texcoord.x;
    float y = texcoord.y;
    float xm1 = x - tex_unit.x;
    float ym1 = y - tex_unit.y;
    vec4 col = texture2D(tex, vec2(x, y));

    float temp = max(col.r,col.g);
    float max_col = max(col.b,temp);

    float temp2 = min(col.r,col.g);
    float min_col = min(col.b,temp2);

    float i = (col.r + col.g + col.b) / 3.0;

    float c = max_col - min_col;
    
    float s = 0.0;

    if(c != 0.0)
    {
        s = 1.0 - min_col / i;
    }
    
    float h = 0.0;
    
    if(max_col == col.r)
    {
        h = (60.0 * abs(col.g - col.b)/c) / 360.0 ;
    }
    else if(max_col  == col.g)
    {
        h = (120.0 + 60.0 * abs(col.b - col.r)/c) / 360.0 ;
    }
    else if(max_col == col.b)
    {
        h = (240.0 + 60.0 * abs(col.r - col.g)/c) / 360.0 ;
    }

    gl_FragColor = vec4(h,s,i,1.0);
}
