#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
varying vec2 texcoord;
uniform vec2 tex_unit;

void main(void) 
{
	vec4 col = vec4(0);
	float total_added = 0.0;
	for(int xoffset = -2; xoffset <= 2; xoffset++)
	{
		for(int yoffset = -2; yoffset <= 2; yoffset++)
		{
			vec2 offset = vec2(xoffset,yoffset);
			float prop = 1.0/(offset.x*offset.x+offset.y*offset.y+1.0);
			total_added += prop;
			col += prop*texture2D(tex,texcoord+offset*tex_unit);
		}
	}
	col /= total_added;
    gl_FragColor = clamp(col,vec4(0),vec4(1));
}
