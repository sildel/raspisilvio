#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
varying vec2 texcoord;
uniform sampler2D mask;

void main(void)
{
    vec4 col = texture2D(mask, texcoord);

    if (col.g > 0.0)
    {
        gl_FragColor = texture2D(tex, texcoord);
    }
    else
    {
        gl_FragColor = vec4(0);
    }
}