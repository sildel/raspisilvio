attribute vec2 vertex;
uniform sampler2D tex;
void main(void)
{
    vec4 col = texture2D(tex, vertex);

    float cos_cita = col.g/col.r;
    float sin_cita = col.b/col.r;
    float tan_cita = col.b/col.g;

    float d = vertex.x * cos_cita + vertex.y * sin_cita;

    if(col.r == 0. || col.r < 0.85)
    {
        cos_cita = 10.0;
        d = 10.0;
    }

    gl_PointSize = col.r * 5.;

    if(col.a == 1.)
    {
        gl_Position = vec4(-1. + tan_cita, -1. + d * 2.,0.0,1.0);
    }
    else
    {
        gl_Position = vec4(tan_cita, -1. + d * 2.,0.0,1.0);
    }

}
