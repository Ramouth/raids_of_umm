#version 330 core

layout(location = 0) in vec2 a_Offset;    // billboard-local: x∈[-0.5,0.5], y∈[0,1]
layout(location = 1) in vec2 a_TexCoord;  // UV in [0,1]

uniform vec3  u_Center;     // world-space base (bottom-centre) of sprite
uniform vec2  u_Size;       // (width, height) in world units
uniform mat4  u_View;
uniform mat4  u_Proj;

// Animation: horizontal spritesheet.  u_NumFrames == 1 → single-frame (static).
uniform int u_Frame;        // 0-based current frame index
uniform int u_NumFrames;    // total columns in the strip

out vec2 v_TexCoord;

void main() {
    // Cylindrical billboard: sprites stand upright in world Y (feet on ground,
    // no tilt from camera elevation) while facing the camera horizontally.
    //
    // camRight: camera's horizontal facing, projected onto the XZ plane so the
    //           sprite turns with the camera without leaning.
    // worldUp: world Y axis, scaled by 1/cos(elevation) so the sprite appears
    //          at its full intended size on screen regardless of camera angle.
    //          Without this correction the sprite foreshortens at steep angles
    //          (74° camera → 27% height). Clamped to avoid extreme stretch.
    vec3 camRight   = normalize(vec3(u_View[0][0], 0.0, u_View[2][0]));
    vec3 worldUp    = vec3(0.0, 1.0, 0.0);
    vec3 camUpWorld = vec3(u_View[0][1], u_View[1][1], u_View[2][1]); // view row 1
    float cosEl     = max(dot(worldUp, camUpWorld), 0.15);  // clamp avoids extreme values
    worldUp         = worldUp / cosEl;                      // elevation-compensated up

    vec3 pos = u_Center
             + camRight * a_Offset.x * u_Size.x
             + worldUp  * a_Offset.y * u_Size.y;

    gl_Position = u_Proj * u_View * vec4(pos, 1.0);

    // Select the correct column in a horizontal spritesheet.
    // a_TexCoord.x is in [0,1] over the full quad; remap to [frame/N, (frame+1)/N].
    float fw = 1.0 / float(u_NumFrames);
    float u  = (a_TexCoord.x + float(u_Frame)) * fw;
    // Flip V: PNG row 0 is the top of the image; OpenGL UV v=0 is the bottom.
    v_TexCoord = vec2(u, 1.0 - a_TexCoord.y);
}
