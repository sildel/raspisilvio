varying vec2 texcoord;
uniform sampler2D tex;

void main(void)
{
    gl_FragColor = texture2D(tex, texcoord);

    gl_FragColor.b = gl_FragColor.a;

    gl_FragColor.a = 1.0;
}
