#include lightingUniforms.glsl

vec3 blinnLight(vec3 P, vec3 N, vec3 kd_tex, vec3 ks_tex, uint materialIndex)
{
	const vec3 V = normalize(eyePos - P);
	
	const Material material = materials[materialIndex];
	const vec3 EmissiveColor = material.EmissiveColor;
	const vec3 ka = material.ka;
	const vec3 kd = material.kd;
	const vec3 ks = material.ks;
	const float ns = material.ns;

	vec3 i_local = vec3(0,0,0);
	i_local += EmissiveColor;
	i_local += globalAmbientColor * ka;

	for(int i = 0; i < 16; ++i)
	{
		if(!lights[i].isActive)
			continue;

		const vec3 L = (lights[i].type == 1) ? 
			normalize(lights[i].direction) : 
			normalize(lights[i].position - P);

		const float ndotl = dot(N, L);
		const vec3 H = normalize(L + V);
		const float ndoth = dot(N, H);
		const float Llen = length(lights[i].position - P);
		const float Attenuation = min(
				1/(
					lightAttenuation[0] + 
					lightAttenuation[1] * Llen + 
					lightAttenuation[2] * Llen * Llen
				),
				1.0
			);

		const float LdotDir = dot(L, normalize(lights[i].direction));

		float SpotlightEffect = 
			(lights[i].type == 2) ?
				(LdotDir - cos(radians(lights[i].spotlightOuterAngle))) / 
					(cos(radians(lights[i].spotlightInnerAngle)) - 
					cos(radians(lights[i].spotlightOuterAngle))) :
				1;
		SpotlightEffect = pow(SpotlightEffect, lights[i].spotlightFalloff);
		SpotlightEffect = clamp(SpotlightEffect, 0, 1);

		const vec3 kd_total = kd * kd_tex;
		const vec3 ks_total = ks * ks_tex;

		vec3 i_effect = {0,0,0}; // the effect that a single light source has on the object
		i_effect += lights[i].ambientColor * ka;																										// ambient
		i_effect += SpotlightEffect * (lights[i].diffuseColor * kd_total * max(ndotl, 0));					// diffuse
		i_effect += SpotlightEffect * (lights[i].specularColor * ks_total * pow(max(ndoth, 0), ns));// specular
		i_effect *= (lights[i].type == 1)? 1 : Attenuation;																					// Attenuation

		i_local += i_effect;
	}

	return i_local;
}