attribute vec2 vertex;
attribute vec2 top_left;
varying vec2 texcoord;

void main(void)
{
    texcoord = 0.5 * (vertex + 1.0);
    gl_Position = vec4(top_left + 0.5 * (vertex + vec2(1.0, - 1.0)), 0.0, 1.0);
}
