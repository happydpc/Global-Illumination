#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 3) uniform UniformBufferObjectLights
{
	vec4 camPos;
	
	vec4 dirDirLight;
	vec4 colorDirLight;

	float usePCF;
	float ambient;
} uboLights;

layout(binding = 4) uniform sampler2D texAlbedo;
layout(binding = 5) uniform sampler2D texNormal;
layout(binding = 6) uniform sampler2D texRoughness;
layout(binding = 7) uniform sampler2D texMetal;
layout(binding = 8) uniform sampler2D texAO;

layout(binding = 9) uniform sampler2D rsmPosition;
layout(binding = 10) uniform sampler2D rsmNormal;
layout(binding = 11) uniform sampler2D rsmFlux;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in mat3 tbn;
layout(location = 6) in vec4 posLightSpace;

layout(location = 0) out vec4 outColor;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

const float PI = 3.14159265359;

vec2 poissonDisk[64] = vec2[]( 
	vec2( 0.063326, 0.142369 ),
	vec2( 0.155736, 0.065157 ),
	vec2( 0.170019, -0.040254 ),
	vec2( 0.203528, 0.214331 ),
	vec2( 0.222999, -0.215125 ),
	vec2( -0.098422, -0.295755 ),
	vec2( 0.099094, -0.308023 ),
	vec2( 0.034028, 0.325968 ),
	vec2( 0.039766, -0.3961 ),
	vec2( 0.175817, 0.382366 ),
	vec2( 0.421003, 0.02707 ),
	vec2( -0.44084, 0.137486 ),
	vec2( 0.487472, -0.063082 ),
	vec2( 0.464034, -0.188818 ),
	vec2( 0.470016, 0.217933 ),
	vec2( 0.200476, 0.49443 ),
	vec2( -0.075838, -0.529344 ),
	vec2( 0.385784, -0.393902 ),
	vec2( 0.503098, -0.308878 ),
	vec2( 0.062655, -0.611866 ),
	vec2( -0.467574, -0.405438 ),
	vec2( -0.178564, -0.596057 ),
	vec2( -0.149693, 0.605762 ),
	vec2( 0.50444, 0.372295 ),
	vec2( 0.364483, 0.511704 ),
	vec2( 0.634388, -0.049471 ),
	vec2( 0.643361, 0.164098 ),
	vec2( 0.315226, -0.604297 ),
	vec2( -0.688894, 0.007843 ),
	vec2( -0.620106, -0.328104 ),
	vec2( 0.678884, -0.204688 ),
	vec2( 0.078707, -0.715323 ),
	vec2( -0.667531, 0.32609 ),
	vec2( -0.545396, 0.538133 ),
	vec2( -0.772454, -0.090976 ),
	vec2( 0.001801, 0.780328 ),
	vec2( 0.69396, -0.366253 ),
	vec2( 0.64568, 0.49321 ),
	vec2( 0.566637, 0.605213 ),
	vec2( -0.299417, 0.791925 ),
	vec2( -0.248268, -0.814753 ),
	vec2( -0.817194, -0.271096 ),
	vec2( -0.494552, -0.711051 ),
	vec2( -0.613392, 0.617481 ),
	vec2( -0.146886, -0.859249 ),
	vec2( -0.016205, -0.872921 ),
	vec2( 0.751946, 0.453352 ),
	vec2( -0.69689, -0.549791 ),
	vec2( 0.789239, -0.419965 ),
	vec2( -0.084078, 0.898312 ),
	vec2( 0.145177, -0.898984 ),
	vec2( -0.885922, 0.215369 ),
	vec2( -0.780145, 0.486251 ),
	vec2( 0.488876, -0.783441 ),
	vec2( 0.724479, -0.580798 ),
	vec2( 0.612476, 0.705252 ),
	vec2( 0.391522, 0.849605 ),
	vec2( 0.354411, -0.88757 ),
	vec2( -0.371868, 0.882138 ),
	vec2( -0.578845, -0.768792 ),
	vec2( -0.651784, 0.717887 ),
	vec2( -0.705374, -0.668203 ),
	vec2( 0.034211, 0.97998 ),
	vec2( 0.97705, -0.108615 )
);

vec2 rsmDiskSampling[151] = vec2[](
	vec2(-0.2105855460826458,0.44847103832481405),
	vec2(-0.3249367874595762,0.45194211733208256),
	vec2(-0.2215745524228665,0.24943447347438874),
	vec2(-0.060082623613624264,0.28588622282467613),
	vec2(-0.10876121836372676,0.1516908621362929),
	vec2(0.029608993270258344,0.36225092203424447),
	vec2(-0.4416077740215665,0.5922474888127149),
	vec2(0.09844974909210591,0.5293232385931546),
	vec2(-0.242750816128263,0.5652476929932564),
	vec2(0.1849719239883465,0.42269268543818006),
	vec2(0.3025579097363722,0.5054990750455668),
	vec2(-0.6198904961790854,0.4923955674020746),
	vec2(0.1371459445570682,0.2256376247802212),
	vec2(0.07335761833299381,0.060664733131448045),
	vec2(-0.0981114897551485,0.5038558748024538),
	vec2(0.02105320196048943,0.20449635334174143),
	vec2(-0.577615989084985,0.6751017664470389),
	vec2(-0.7047095788535609,0.6703810066791474),
	vec2(-0.3461478915533044,0.2767411968699174),
	vec2(0.22826800325817365,0.6885961509356942),
	vec2(0.008557608781530446,0.6846674257651739),
	vec2(-0.7457514972966308,0.5525938412112139),
	vec2(-0.12675177599317122,0.03807380965906315),
	vec2(0.26001862548878574,0.9017228978398559),
	vec2(-0.12749254424512912,0.6394141870327132),
	vec2(-0.25490982834883025,-0.009623207028237046),
	vec2(-0.2832188417687387,0.7080442976582488),
	vec2(-0.5568853398254002,0.8281675565698587),
	vec2(-0.4502169909230217,0.1983838328508294),
	vec2(-0.3374755253275854,0.12304984569933919),
	vec2(0.28022304577413726,0.19855055835921753),
	vec2(-0.21036466694065437,0.9159129231359),
	vec2(0.18360415112566697,0.02168652914349556),
	vec2(-0.4584645969682012,0.3302427234342433),
	vec2(-0.4479511888159913,0.7410125839426094),
	vec2(0.3991624033194776,0.7988858591043992),
	vec2(-0.721662130539607,0.39995242005235676),
	vec2(-0.3442261159253708,0.8599343768096765),
	vec2(-0.8466679858222527,0.36459222475755637),
	vec2(-0.45878496236317035,0.47334713275894513),
	vec2(-0.4372446791568142,-0.0165798426817938),
	vec2(0.02100915885650645,-0.06981332644045746),
	vec2(0.13110358045707327,0.8247787987084165),
	vec2(0.00823456530315858,0.8107451419490457),
	vec2(0.42797771436580456,0.5587115156373583),
	vec2(0.32214857014394704,0.33031482110713184),
	vec2(-0.22137684848340067,0.1136363932619775),
	vec2(-0.14193395356177185,-0.08604938105381399),
	vec2(-0.5536753158083249,-0.18073175280365916),
	vec2(-0.5070399904334678,0.08689622459455082),
	vec2(0.52028114635073,0.3812556209224749),
	vec2(-0.08344310105240482,0.95881586699393),
	vec2(-0.5862241280594775,0.31599501752459136),
	vec2(-0.7424917104785005,-0.14495274618636356),
	vec2(-0.71268208479312,0.18074542096312163),
	vec2(0.108990027583584,-0.15666655867133028),
	vec2(-0.015166808630370787,-0.2487589023599489),
	vec2(0.13234925413927323,0.9492511038584823),
	vec2(-0.12322536949396168,0.7992904731603836),
	vec2(0.36403836196961614,0.6617387426658699),
	vec2(0.39955450741769316,0.41547955572951234),
	vec2(-0.5664427084973017,0.18679436589332576),
	vec2(-0.6944101245020164,-0.01343990502836212),
	vec2(0.5625324116044776,0.5279964765578082),
	vec2(0.3782464905914933,0.12643160642731033),
	vec2(-0.8454075321944396,0.014131655121489839),
	vec2(0.4206171451921148,0.25475356609725885),
	vec2(-0.9192606507897227,-0.1425594778539232),
	vec2(-0.9654723633960183,0.19512401454226325),
	vec2(0.3301371414109351,-0.06358517986619083),
	vec2(-0.16766134181208847,-0.2189845370715473),
	vec2(-0.542947723738074,-0.061891341033254776),
	vec2(0.3145600952238312,-0.18962449885503818),
	vec2(-0.3877834273727072,-0.11963285486335329),
	vec2(-0.6912736594144696,-0.2576592094385691),
	vec2(0.5299377470642048,0.6429034882840958),
	vec2(-0.9092290171229506,-0.2576042816799101),
	vec2(-0.756598574200095,-0.3783062261042447),
	vec2(-0.3359282441771242,-0.22535842315392463),
	vec2(-0.19113437626591812,-0.37544518075404265),
	vec2(-0.8316639544686806,0.24888528214021544),
	vec2(-0.3842335024447435,-0.3707398001959058),
	vec2(0.0833362819102259,-0.36313489626173856),
	vec2(-0.04703622100738214,-0.36769015058577315),
	vec2(0.492677762334105,0.10573129785820412),
	vec2(0.5145026631513785,0.7862005812142474),
	vec2(-0.0805949863665465,-0.5098442295614739),
	vec2(-0.36144986019027736,-0.5221245642722397),
	vec2(-0.5338520212539983,-0.41540245649168794),
	vec2(0.08799899906907926,-0.5327065307354413),
	vec2(0.6051010684289804,0.03429146108464676),
	vec2(-0.5199138995020798,-0.5290112928940491),
	vec2(-0.21573024720399891,-0.5777225511384059),
	vec2(-0.48401194020273963,-0.28195487161906563),
	vec2(-0.2523937202004908,-0.13494585553274363),
	vec2(0.6583622019076383,0.25357468334574595),
	vec2(0.7332514662646883,0.5471373800529391),
	vec2(0.1820533681249794,-0.25589649723524566),
	vec2(0.4224076053791508,-0.29088682980126956),
	vec2(0.7177903741554872,0.07235396433442842),
	vec2(-0.4771029660046049,-0.6719876518886342),
	vec2(0.2534785901118639,-0.40897467057733083),
	vec2(-0.27600712311713327,-0.680828645289126),
	vec2(-0.6185492815793612,-0.7453116089045722),
	vec2(-0.29124157302402065,-0.857368819598208),
	vec2(0.6478925289192881,0.6345225707327742),
	vec2(0.5606766211201131,-0.16747590407909985),
	vec2(-0.6544148295496951,-0.44189162385444347),
	vec2(0.7660305835364689,-0.1163440033664408),
	vec2(-0.984396930296487,-0.00434290617490074),
	vec2(0.43218518472597256,-0.17091863644713534),
	vec2(-0.04413960266224226,-0.703047548286027),
	vec2(0.5362856037867374,-0.30318221730699),
	vec2(0.8502947919017667,0.1312310157856249),
	vec2(-0.6479918401665218,-0.6051526163645965),
	vec2(0.4240350465627807,0.00822566312902584),
	vec2(0.2230332514623785,-0.5431579662998082),
	vec2(0.6464790776805429,0.37436179966037253),
	vec2(0.9224902048831116,0.041580855014303086),
	vec2(-0.4324019065826592,-0.787299608631407),
	vec2(0.13872153774843388,-0.7113018553685244),
	vec2(0.8531859057169597,0.40581471690902804),
	vec2(0.5453953127927622,0.2093370569631816),
	vec2(0.9243895369495867,0.2642707871055614),
	vec2(-0.006129765418066646,-0.8511686376433512),
	vec2(0.27384668647208743,-0.7164307699873447),
	vec2(0.20004837507761009,-0.8351498888499372),
	vec2(0.7777056608159021,0.2404310438231101),
	vec2(-0.7484978798285156,-0.5396097698216022),
	vec2(-0.8403766405915871,-0.4558144551108865),
	vec2(0.3522383577039254,-0.4758041359031191),
	vec2(0.783890871969178,-0.34279882440569154),
	vec2(-0.18978613691531687,-0.784090953537453),
	vec2(-0.09560238051589443,-0.9250374202209017),
	vec2(0.048502964663310166,-0.9549042447300811),
	vec2(0.6456628770495716,-0.33756595771091413),
	vec2(0.9593865924353975,-0.12327064637742902),
	vec2(0.6783583896965726,-0.4787960858888588),
	vec2(0.405881740124189,-0.6279578719333552),
	vec2(0.8224294731607125,-0.22993697489365883),
	vec2(0.5268791482035144,-0.05272822133058308),
	vec2(0.6580553825480155,-0.07591007732029698),
	vec2(0.5299876015032885,-0.4972122527133582),
	vec2(0.7208672506325395,-0.5952659848227987),
	vec2(0.5432340182562456,-0.652824644026825),
	vec2(0.6747160604935316,-0.19158382758078019),
	vec2(0.2201129257859169,-0.1252318363959143),
	vec2(0.7993354103716619,-0.46184850776665365),
	vec2(0.6856655239366938,-0.7049805434613721),
	vec2(0.3666048733591001,-0.803469722802703),
	vec2(0.48271502610609507,-0.8050882593415389)
);

float random(vec3 seed, int i){
	vec4 seed4 = vec4(seed,i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

void main() 
{
	vec3 albedo = texture(texAlbedo, fragTexCoord).xyz;
	float roughness = texture(texRoughness, fragTexCoord).x;
	float metallic = texture(texMetal, fragTexCoord).x;
	float ao = texture(texAO, fragTexCoord).x;
	vec3 normal = (texture(texNormal, fragTexCoord).xyz * 2.0 - vec3(1.0)) * tbn;

	vec3 N = normalize(normal); 
    vec3 V = normalize(uboLights.camPos.xyz - worldPos);
	vec3 R = reflect(-V, N);  

	vec3 F0 = vec3(0.04); 
    F0 = mix(F0,albedo, metallic);
	
	vec3 Lo = vec3(0.0);
	
	// calculate per-light radiance
	vec3 L = normalize(-uboLights.dirDirLight.xyz);
	vec3 H = normalize(V + L);
	vec3 radiance     = uboLights.colorDirLight.xyz;        
	
	// cook-torrance brdf
	float NDF = DistributionGGX(N, H, roughness);        
	float G   = GeometrySmith(N, V, L, roughness);      
	vec3 F    = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);       
	
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;	  
	
	vec3 nominator    = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0); 
	vec3 specular     = nominator / max(denominator, 0.001);
		
	// add to outgoing radiance Lo
	float NdotL = max(dot(N, L), 0.0);                
	Lo += (kD * albedo / PI + specular) * radiance * NdotL;

	vec3 ambient = vec3(uboLights.ambient) * albedo;

    vec3 color = ambient + Lo;
	vec3 indirectRSM = vec3(0.0);

	vec4 projCoords = posLightSpace / posLightSpace.w; 
	vec2 rsmTextexSize = vec2(1.0 / 512.0, 1.0 / 512.0);
	for(int i = 0; i < 151; i++)
	{
		//int index = int(float(64)*random(worldPos.xyz, i))%64;
		//vec4 rsmProjCoords = projCoords + vec4(rsmDiskSampling[i] * 0.15, 0.0, 0.0);
		//vec4 randomProjCoords = projCoords + vec4(rsmTextexSize.x * i, rsmTextexSize.y * j, 0.0, 0.0);
		vec4 rsmProjCoords = projCoords + vec4(rsmDiskSampling[i] * 0.03, 0.0, 0.0);

		vec3 indirectLightPos = texture(rsmPosition, rsmProjCoords.xy).rgb;
		vec3 indirectLightNorm = texture(rsmNormal, rsmProjCoords.xy).rgb;
		vec3 indirectLightFlux = texture(rsmFlux, rsmProjCoords.xy).rgb;
		
		vec3 r = worldPos - indirectLightPos;
		float distP2 = dot( r, r );
		
		vec3 emission = indirectLightFlux * (max(0.0, dot(indirectLightNorm, r)) * max(0.0, dot(N, -r)));
	    emission *= (rsmDiskSampling[i].x * rsmDiskSampling[i].x) / (distP2 * distP2);

		indirectRSM += emission;
	}
	indirectRSM *= 0.1;
	color += indirectRSM;
	
    float exposure = 0.5;
    color = vec3(1.0) - exp(-color * exposure);
	color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, texture(texAlbedo, fragTexCoord).a);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}