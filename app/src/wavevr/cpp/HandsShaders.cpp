namespace crow {
    static const char *handDepthFragment = R"SHADER(
#version 100
precision VRB_FRAGMENT_PRECISION float;

uniform sampler2D u_texture0;
varying vec4 v_color;
varying vec2 v_uv;
varying vec2 v_uv2;
uniform vec4 graColorA;
uniform vec4 graColorB;
uniform float opacity;

void main() {
  vec2 v2fCoord1 = v_uv;
  vec2 v2fCoord2 = v_uv2;

  float clampResult114 = clamp((0.59 + (0.59 - (1.0 - v2fCoord2.y)) / .38), 0.0, 1.0);
  vec4 lerpResult100 = mix(graColorA, graColorB, clampResult114);
  float color104 = 0.0;
  float color102 = 1.0;
  float smoothstepResult103 = smoothstep(-0.05, 1.0, 1.0 - v2fCoord2.y);
  float lerpResult105 = mix(color104, color102, smoothstepResult103);
  float tex2DNode92 =  texture2D(u_texture0, v2fCoord1).r;
  vec4 oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, lerpResult105 * opacity * tex2DNode92);
  gl_FragColor = oColor;
}

)SHADER";

    const char *GetHandDepthFragment() { return handDepthFragment; }

    static const char *handContouringFragment = R"SHADER(
#version 100
precision VRB_FRAGMENT_PRECISION float;

uniform sampler2D u_texture0;
uniform vec4 graColorA;
uniform vec4 graColorB;
uniform float opacity;
varying vec2 v_uv;
varying vec2 v_uv2;

void main() {
  vec2 v2fCoord1 = v_uv;
  vec2 v2fCoord2 = v_uv2;

  float clampResult114 = clamp((0.59 + (0.59 - (1.0 - v2fCoord2.y)) / 0.38), 0.0, 1.0);
  vec4  lerpResult100  = mix(graColorA, graColorB, clampResult114);
  float tex2DNode92    = texture2D(u_texture0, v2fCoord1).r;
  vec4 oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, opacity * tex2DNode92);
  gl_FragColor = oColor;
}

)SHADER";

    const char *GetHandContouringFragment() { return handContouringFragment; }

} // namespace crow
