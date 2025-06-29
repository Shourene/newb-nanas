#ifndef CLOUDS_H
#define CLOUDS_H

#include "noise.h"
#include "sky.h"

// simple clouds 2D noise
float cloudNoise2D(vec2 p, highp float t, float rain) {
  t *= NL_CLOUD1_SPEED;
  p += t;
  p.y += 3.0*sin(0.3*p.x + 0.1*t);

  vec2 p0 = floor(p);
  vec2 u = p-p0;
  u *= u*(3.0-2.0*u);
  vec2 v = 1.0-u;

  float n = mix(
    mix(rand(p0),rand(p0+vec2(1.0,0.0)), u.x),
    mix(rand(p0+vec2(0.0,1.0)),rand(p0+vec2(1.0,1.0)), u.x),
    u.y
  );
  n *= 0.5 + 0.5*sin(p.x*0.6 - 0.5*t)*sin(p.y*0.6 + 0.8*t);
  n = min(n*(1.0+rain), 1.0);
  return n*n;
}

// simple clouds
vec4 renderCloudsSimple(nl_skycolor skycol, vec3 pos, highp float t, float rain) {
  pos.xz *= NL_CLOUD1_SCALE;
  float d = cloudNoise2D(pos.xz, t, rain);
  vec4 col = vec4(skycol.horizonEdge + skycol.zenith, smoothstep(0.1,0.6,d));
  col.rgb += 1.5*dot(col.rgb, vec3(0.3,0.4,0.3))*smoothstep(0.6,0.2,d)*col.a;
  col.rgb *= 1.0 - 0.8*rain;
  return col;
}

// rounded clouds

// rounded clouds 3D density map
float cloudDf(vec3 pos, float rain, vec2 boxiness) {
  boxiness *= 0.999;
  vec2 p0 = floor(pos.xz);
  vec2 u = max((pos.xz-p0-boxiness.x)/(1.0-boxiness.x), 0.0);
  u *= u*(3.0 - 2.0*u);

  vec4 r = vec4(rand(p0), rand(p0+vec2(1.0,0.0)), rand(p0+vec2(1.0,1.0)), rand(p0+vec2(0.0,1.0)));
  r = smoothstep(0.1001+0.2*rain, 0.1+0.2*rain*rain, r); // rain transition

  float n = mix(mix(r.x,r.y,u.x), mix(r.w,r.z,u.x), u.y);

  // round y
  n *= 1.0 - 1.5*smoothstep(boxiness.y, 2.0 - boxiness.y, 2.0*abs(pos.y-0.5));

  n = max(1.26*(n-0.2), 0.0);
  //n = smoothstep(0.2, 1.0, n);
  n *= n*(3.0 - 2.0*n);
  return n;
}

vec4 renderCloudsRounded(
    vec3 vDir, vec3 vPos, float rain, float time, vec3 horizonCol, vec3 zenithCol,
    const int steps, const float thickness, const float thickness_rain, const float speed,
    const vec2 scale, const float density, const vec2 boxiness
) {
  float height = 7.0*mix(thickness, thickness_rain, rain);
  float stepsf = float(steps);

  // scaled offset ray
  vec3 deltaP;
  deltaP.y = 1.0;
  deltaP.xz = height*scale*vDir.xz/(0.02 + 0.98*abs(vDir.y));

  // local cloud pos
  vec3 pos;
  pos.y = 0.0;
  pos.xz = scale*(vPos.xz + vec2(1.0, 0.5)*(time*speed));
  pos += deltaP;

  deltaP /= -stepsf;

  // alpha, gradient
  vec2 d = vec2(0.0, 1.0);
  for (int i = 1; i <= steps; i++) {
    float m = cloudDf(pos, rain, boxiness);

    d.x += m;
    d.y = mix(d.y, pos.y, m);

    pos += deltaP;
  }

  d.x *= smoothstep(1.0, 1.0, d.x); // 0.03, 0.1
  d.x /= (stepsf/density) + d.x;

  if (vPos.y > 0.0) { // view from bottom
    d.y = 1.0 - d.y;
  }

  d.y = 1.0 - 1.15*d.y*d.y;

  // detect time
  float night = max(1.0-3.0*max(horizonCol.b, horizonCol.g), 0.0);
  float dusk = smoothstep(0.1, 0.5, max(horizonCol.r - horizonCol.b, 0.0)*max(horizonCol.r - horizonCol.g*0.5, 0.0));

  // color blending
  vec4 col = vec4(horizonCol*1.6, d.x);
  col.rgb += (zenithCol + 0.99*horizonCol)*d.y;
  col.rgb *= 1.0 - 0.5*rain;
  col.rgb += mix(horizonCol, vec3(0.55, 0.20, 0.12), dusk);

  return col;
}

float cloudsNoiseVr(vec2 p, float t) {
  float n = fastVoronoi2(p + t, 1.8);
  n *= fastVoronoi2(3.0*p + t, 1.5);
  n *= fastVoronoi2(9.0*p + t, 0.4);
  n *= fastVoronoi2(27.0*p + t, 0.1);
  //n *= fastVoronoi2(82.0*pos + t, 0.02); // more quality
  return n*n;
}

vec4 renderClouds(vec2 p, float t, float rain, vec3 horizonCol, vec3 zenithCol, const vec2 scale, const float velocity, const float shadow) {
  p *= scale;
  t *= velocity;

  // layer 1
  float a = cloudsNoiseVr(p, t);
  float b = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // layer 2
  p = 1.4 * p.yx + vec2(7.8, 9.2);
  t *= 0.5;
  float c = cloudsNoiseVr(p, t);
  float d = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // higher = less clouds thickness
  // lower separation betwen x & y = sharper
  vec2 tr = vec2(0.6, 0.7) - 0.12*rain;
  a = smoothstep(tr.x, tr.y, a);
  c = smoothstep(tr.x, tr.y, c);

  // shadow
  b *= smoothstep(0.2, 0.8, b);
  d *= smoothstep(0.2, 0.8, d);

  vec4 col;
  col.a = a + c*(1.0-a);
  col.rgb = horizonCol + horizonCol.ggg;
  col.rgb = mix(col.rgb, 0.5*(zenithCol + zenithCol.ggg), shadow*mix(b, d, c));
  col.rgb *= 1.0-0.7*rain;

  return col;
}

float fbm2D(vec2 p) {
  float sum = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 3; i++) {
    sum += amp*fractNoise2D(p*freq);
    freq *= 2.0;
    amp *= 0.5;
  }
  return sum;
}

// need improvement
vec3 renderCloudsEnd(nl_skycolor skycol, vec3 v, float t) {
  t *= NL_END_CLOUD_SPEED;
  // Layer 2D noise in XZ, offset Y for volumetric effect
  float n0 = fbm2D(v.xz*2.4 + vec2(-5.0, t*0.5 - v.y*0.5));
  float n1 = fbm2D(v.xz*1.6 + vec2(0.0, t*0.10 + v.y*0.5));
  float density = mix(n0, n1, 0.5);

  float base = smoothstep(0.25, 1.0, density);
  float horizon = smoothstep(0.0, 0.4, v.y);

  vec3 sky = mix(skycol.zenith, skycol.horizon, horizon);
  vec3 col = mix(sky + vec3(0.4078, 0.2039, 0.5451), sky, 0.5);

  float vertical = smoothstep(0.1, 0.95, v.y);
  col = mix(col, col, vertical);

  float alpha = NL_END_CLOUD_DENSITY*(base*(horizon*2.0));
  return col*alpha;
}

// aurora is rendered on clouds layer
#ifdef NL_AURORA
vec4 renderAurora(vec3 p, float t, float rain, vec3 FOG_COLOR) {
  t *= NL_AURORA_VELOCITY;
  p.xz *= NL_AURORA_SCALE;
  p.xz += 0.05*sin(p.x*4.0 + 20.0*t);

  float d0 = sin(p.x*0.1 + t + sin(p.z*0.2));
  float d1 = sin(p.z*0.1 - t + sin(p.x*0.2));
  float d2 = sin(p.z*0.1 + 1.0*sin(d0 + d1*2.0) + d1*2.0 + d0*1.0);
  d0 *= d0; d1 *= d1; d2 *= d2;
  d2 = d0/(1.0 + d2/NL_AURORA_WIDTH);

  float mask = (1.0-0.8*rain)*max(1.0 - 4.0*max(FOG_COLOR.b, FOG_COLOR.g), 0.0);
  return vec4(NL_AURORA*mix(NL_AURORA_COL1,NL_AURORA_COL2,d1),1.0)*d2*mask;
}
#endif

#endif
