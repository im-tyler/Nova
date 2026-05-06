shader_type spatial;
render_mode cull_disabled;

uniform vec4 albedo_color : source_color = vec4(0.2, 0.5, 0.9, 1.0);
uniform float roughness : hint_range(0.0, 1.0) = 0.7;

void fragment() {
    ALBEDO = albedo_color.rgb;
    ROUGHNESS = roughness;

    // Flip normal for back faces so both sides are lit
    if (!FRONT_FACING) {
        NORMAL = -NORMAL;
    }
}
