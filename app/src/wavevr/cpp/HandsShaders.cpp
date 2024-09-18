namespace crow {
  static const char *handDepthFragment = R"SHADER(
#version 100
precision VRB_FRAGMENT_PRECISION float;

uniform sampler2D u_texture0;
varying vec4 v_color;
varying vec2 v_uv;

void main() {
  vec4 textureColor = texture2D(u_texture0, v_uv);
  vec4 startColor = vec4(v_color.xyz, 1.0);
  vec4 endColor = vec4(v_color.xyz, 0.4);
  float value = (textureColor.x + textureColor.y + textureColor.z) / 3.0;
  gl_FragColor =  startColor * value + endColor * (1.0 - value);
}

)SHADER";

  const char *GetHandDepthFragment() { return handDepthFragment; }

} // namespace crow
