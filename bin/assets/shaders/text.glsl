#VERTEX
#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in vec3 in_uv;

out vec4 pass_color;
out vec3 pass_uv;
out float pass_screen_px_range;

layout (std140) uniform camera
{
    mat4 view_projection_matrix;
};

void main()
{
    pass_color = in_color;
    pass_uv = in_uv;
    pass_screen_px_range = in_position.z;
    gl_Position = view_projection_matrix * vec4(in_position.xy, 0.0, 1.0);
}

#FRAGMENT
#version 330 core

in vec4 pass_color;
in vec3 pass_uv;
in float pass_screen_px_range;

layout (location = 0) out vec4 color;

uniform sampler2D textures[16];

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd;
    switch (int(pass_uv.z)) {
         case 0: msd = texture(textures[0], pass_uv.xy).rgb; break; 
         case 1: msd = texture(textures[1], pass_uv.xy).rgb; break;  
         case 2: msd = texture(textures[2], pass_uv.xy).rgb; break; 
         case 3: msd = texture(textures[3], pass_uv.xy).rgb; break; 
         case 4: msd = texture(textures[4], pass_uv.xy).rgb; break; 
         case 5: msd = texture(textures[5], pass_uv.xy).rgb; break; 
         case 6: msd = texture(textures[6], pass_uv.xy).rgb; break; 
         case 7: msd = texture(textures[7], pass_uv.xy).rgb; break; 
         case 8: msd = texture(textures[8], pass_uv.xy).rgb; break; 
         case 9: msd = texture(textures[9], pass_uv.xy).rgb; break; 
         case 10: msd = texture(textures[10], pass_uv.xy).rgb; break; 
         case 11: msd = texture(textures[11], pass_uv.xy).rgb; break; 
         case 12: msd = texture(textures[12], pass_uv.xy).rgb; break; 
         case 13: msd = texture(textures[13], pass_uv.xy).rgb; break; 
         case 14: msd = texture(textures[14], pass_uv.xy).rgb; break;    
         case 15: msd = texture(textures[15], pass_uv.xy).rgb; break; 
    }    


    float sd = median(msd.r, msd.g, msd.b);
    float screen_px_distance = pass_screen_px_range * (sd - 0.5);
    float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);
    color = mix(vec4(0.0), pass_color, opacity);
}