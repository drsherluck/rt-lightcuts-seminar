#version 460

layout(push_constant) uniform constants
{
    float znear;
    float zfar;
};

void main()
{
    gl_FragDepth = znear * zfar / (zfar + gl_FragCoord.z * (znear - zfar));
}
