#VERTEX
#version 330 core

layout (location = 0) in vec3 in_world_position;
layout (location = 1) in vec2 in_local_position;
layout (location = 2) in vec4 in_color;
layout (location = 3) in vec3 in_uv;

out vec4 pass_color;
out vec3 pass_uv;
out vec2 pass_position;

layout (std140) uniform camera
{
    mat4 view_projection_matrix;
};

void main()
{
    pass_color = in_color;
    pass_uv = in_uv;
    pass_position = in_local_position;
    gl_Position = view_projection_matrix * vec4(in_world_position, 1.0);
}

#FRAGMENT
#version 330 core

in vec4 pass_color;
in vec3 pass_uv;
in vec2 pass_position;

layout (location = 0) out vec4 color;

uniform sampler2D textures[16];

void main()
{
    if (length(pass_position) > 1.0)
        discard;

    vec4 sampled_color;
    switch (int(pass_uv.z)) {
         case 0: sampled_color = texture(textures[0], pass_uv.xy); break; 
         case 1: sampled_color = texture(textures[1], pass_uv.xy); break;  
         case 2: sampled_color = texture(textures[2], pass_uv.xy); break; 
         case 3: sampled_color = texture(textures[3], pass_uv.xy); break; 
         case 4: sampled_color = texture(textures[4], pass_uv.xy); break; 
         case 5: sampled_color = texture(textures[5], pass_uv.xy); break; 
         case 6: sampled_color = texture(textures[6], pass_uv.xy); break; 
         case 7: sampled_color = texture(textures[7], pass_uv.xy); break; 
         case 8: sampled_color = texture(textures[8], pass_uv.xy); break; 
         case 9: sampled_color = texture(textures[9], pass_uv.xy); break; 
         case 10: sampled_color = texture(textures[10], pass_uv.xy); break; 
         case 11: sampled_color = texture(textures[11], pass_uv.xy); break; 
         case 12: sampled_color = texture(textures[12], pass_uv.xy); break; 
         case 13: sampled_color = texture(textures[13], pass_uv.xy); break; 
         case 14: sampled_color = texture(textures[14], pass_uv.xy); break;    
         case 15: sampled_color = texture(textures[15], pass_uv.xy); break; 
    }
    color = pass_color * sampled_color;
}