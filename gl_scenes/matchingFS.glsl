#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
uniform vec3 lower;
uniform vec3 upper;

varying vec2 texcoord;

void main(void)
{
    vec4 rgba_col = texture2D(tex, texcoord);
    vec4 thresh_col = vec4(0.0,0.0,0.0,1.0);

    float temp = max(rgba_col.r,rgba_col.g);
    float temp2 = min(rgba_col.r,rgba_col.g);
    float temp3 = min(rgba_col.b,temp2);
    float v = max(rgba_col.b,temp);
    float s = 0.0;
    float temp4 = v - temp3;
    float h = 0.0;
    if(v != 0.0)
    {
        s = temp4 / v;
    }
    if(v == rgba_col.r)
    {
        h = 60.0 * abs(rgba_col.g - rgba_col.b)/temp4 ;
    }
    else if(v  == rgba_col.g)
    {
        h = 120.0 + 60.0 * abs(rgba_col.b - rgba_col.r)/temp4 ;
    }
    else if(v == rgba_col.b)
    {
        h = 240.0 + 60.0 * abs(rgba_col.r - rgba_col.g)/temp4 ;
    }
    if((h >= lower.x) && (h <= upper.x) && (s >= lower.y) && (s <= upper.y) && (v >= lower.z) && (v <= upper.z) )
    {
        thresh_col = vec4(1.0) ;
    }
    gl_FragColor = thresh_col;
}
