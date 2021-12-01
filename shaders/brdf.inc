
vec3 f_schlick(vec3 f0, float u)
{
    return f0 + (vec3(1.0f) - f0) * pow(1.0f - u, 5.0f);
}

float d_ggx(in float a2, float NdotH)
{
    float d = PI*pow((a2 - 1.0f) * (NdotH * NdotH) + 1.0f, 2.0f);
    return a2/(d + 1e-5f);
}

float g_smith(float alpha, float NdotV, float NdotL)
{
    float k = alpha/2;
    float g1 = NdotL/(NdotL*(1.0f - k) + k);
    float g2 = NdotV/(NdotV*(1.0f - k) + k);
    return g1 * g2;
}
