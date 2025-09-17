RaytracingAccelerationStructure sceneAS : register(t0);
RWTexture2D<float4> output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    uint3 dispatchIndex = DispatchRaysIndex();
    output[dispatchIndex.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f); // Clear output to black
}

struct Payload
{
    bool hit;
};

[shader("miss")]
void MissShader(inout Payload payload)
{
    payload.hit = false;
}


[shader("closesthit")]
void ClosestHitShader(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hit = true;
}
