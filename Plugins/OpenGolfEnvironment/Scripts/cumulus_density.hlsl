// SPDX-License-Identifier: MIT
float2 xy = Position.xy * .00001 - float2(Offset, Offset * .25);
float2 turned = float2(dot(xy, float2(.80, .60)), dot(xy, float2(-.60, .80)));
float weather = Texture3DSampleLevel(Shape, ShapeSampler, float3(turned * .038, .37), 0).r;

// A common condensation level gives each bank a base. Deeper patches grow higher.
float base = .035 + .045 * saturate((weather - .75) * 5.0);
float height = (LayerHeight - base) / (.40 + .25 * saturate((weather - .82) * 8.0));
if (height <= 0.0 || height >= 1.0)
    return 0.0;

float3 p = float3(xy, LayerAltitudeCm * .00001);
float core = Texture3DSampleLevel(Shape, ShapeSampler, p * .085 + float3(.19, .43, .67), 0).r;
float crown = smoothstep(.18, 1.0, height);
float margin = .60 * (core - .88) + .40 * (weather - .88)
             + (Coverage - .35) * .24 - .044 * crown - .044;

if (margin < -.068)
    return 0.0;

// The smaller billows run through the volume, so top and side views have the same detail.
float billow = Texture3DSampleLevel(Shape, ShapeSampler, float3(turned, p.z) * .28 + .41, 0).r;
float erosion = Texture3DSampleLevel(Erosion, ErosionSampler, p * 1.25 + billow * .14, 0).r;
margin += .33 * (billow - .88) + .095 * (erosion - .71);
float foot = smoothstep(0.0, .18, height);
return max(0.0, margin) * foot * (1.0 - crown) * .29;
