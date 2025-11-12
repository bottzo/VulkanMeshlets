#version 460

//layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

//input normal
layout(location=0) in vec3 normal;
layout(location=1) in flat uint meshletID;
layout(location=2) in flat uint meshID;
//lambertian shader
const vec3 lightDir = normalize(vec3(0.0f,1.0f, 1.0f));
const vec3 diffuseCol = vec3(0.5f, 0.5f, 0.0f);
const vec3 ambientCol = vec3(0.0f,  0.0f, 0.2f);

#define MAX_COLORS 10
vec3 meshletColors[MAX_COLORS] = {
  vec3(1,0,0), 
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  vec3(1,1,1)
  };

void main() {
    //outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    //outColor = vec4(normal, 1.0f);
    //outColor = vec4(meshletColors[meshletID%MAX_COLORS],1.0f);
    //outColor = vec4(ambientCol,1) + vec4(diffuseCol * max(dot(normalize(normal), lightDir), 0.0f), 1.0f);
    //outColor = vec4(ambientCol,1) + vec4(meshletColors[meshID%MAX_COLORS] * max(dot(normalize(normal), lightDir), 0.0f), 1.0f);
    outColor = vec4(ambientCol,1) + vec4(mix(meshletColors[meshID%MAX_COLORS], meshletColors[meshletID%MAX_COLORS], 0.3) * max(dot(normalize(normal), lightDir), 0.0f), 1.0f);
}