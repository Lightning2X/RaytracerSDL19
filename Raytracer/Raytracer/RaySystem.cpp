#include "precomp.h"
#include "xmmintrin.h"
#define SIZE SCREEN_WIDTH * SCREEN_HEIGHT

RaySystem::RaySystem(Screen* screen)
{
	this->width = screen->getWidth();
	this->height = screen->getHeight();
}

void RaySystem::init(Scene* scene, Camera* camera) {
	this->scene = scene;
	this->camera = camera;

	shapes = scene->objects;
	shapeSize = shapes.size();
	lights = scene->lights;
	lightSize = lights.size();

	Vec3Df startDir = camera->getRelTopLeft();
	Vec3Df camPos = camera->position;
	xOffset = (float)SCREEN_DIMENSION * 2 / (width - 1);
	yOffset = (float)SCREEN_DIMENSION * 2 / (height - 1);

	startX = _mm256_set1_ps(startDir.get_x());
	startY = _mm256_set1_ps(startDir.get_y());
	startZ = _mm256_set1_ps(startDir.get_z());

	ox = _mm256_set1_ps(camPos.get_x());
	oy = _mm256_set1_ps(camPos.get_y());
	oz = _mm256_set1_ps(camPos.get_z());

	rayLen = _mm256_set1_ps(100);
}

void RaySystem::draw(Pixel* pixelBuffer) {

	// #pragma unroll
	// Initialize ray values
	for (unsigned int i = 0; i < (height * width) / AVX_SIZE; i++)
	{
		float x = i * AVX_SIZE % width;
		float y = i * AVX_SIZE / width; 
		__m256 test = _mm256_setr_ps(1, 0, x + 2 * xOffset, 0, 0, 0, 0, 0);
		__m256 dx = _mm256_add_ps(startX,
			_mm256_setr_ps(x * xOffset, x + 1 * xOffset, x + 2 * xOffset, x + 3 * xOffset, x + 4 * xOffset, x + 5 * xOffset, x + 6 * xOffset, x + 7 * xOffset));
		__m256 dy = _mm256_add_ps(startY,
			_mm256_setr_ps(y * yOffset, y + 1 * yOffset, y + 2 * yOffset, y + 3 * yOffset, y + 4 * yOffset, y + 5 * yOffset, y + 6 * yOffset, y + 7 * yOffset));
		__m256 dz = startZ;
		AvxVector3 norm = normalize(dx, dy, dz); 

		dirX[i] = norm.x;
		dirY[i] = norm.y;
		dirZ[i] = norm.z;
		originX[i] = ox;
		originY[i] = oy;
		originZ[i] = oz;
		length[i] = rayLen; 
	}

	for (unsigned int j = 0; j < (height * width) / AVX_SIZE; j++)
	{
		trace(j, 0);
		r[j] = _mm256_min_ps(r[j], _mm256_setzero_ps());
		g[j] = _mm256_min_ps(r[j], _mm256_setzero_ps());
		b[j] = _mm256_min_ps(r[j], _mm256_setzero_ps());

		// Please check later if this is correct lol
		unsigned int startIndex = j * AVX_SIZE * 4;
		for (unsigned int c = 0; c < 8; c++) {
			*(pixelBuffer + startIndex) = r[j].m256_f32[c];
			*(pixelBuffer + startIndex + 1) = g[j].m256_f32[c];
			*(pixelBuffer + startIndex + 2) = b[j].m256_f32[c];
			*(pixelBuffer + startIndex + 3) = 0;
			startIndex += 4;
		} 
	}

	/*
	Vec3Df rayStartDir = camera->getRelTopLeft();
	float xOffset = (float)SCREEN_DIMENSION * 2 / (width - 1);
	float yOffset = (float)SCREEN_DIMENSION * 2 / (height - 1);
	#pragma unroll
	for (unsigned int y = 0; y < height; y++)
	{
		for (unsigned int x = 0; x < width; x++)
		{
			// Make ray
			Vec3Df direction = rayStartDir + Vec3Df(x * xOffset, y * yOffset, 0);
			direction = normalize_vector(direction);
			Ray r = { camera->position, direction, 100 };
			// Check for hits
			Vec3Df argb = trace(r,0) * Vec3Df(255);

			// Convert color
			unsigned int xy = x * 4 + (y * width) * 4;
			*(pixelBuffer + xy) = std::min((int)argb.extract(0), 255);
			*(pixelBuffer + xy+1) = std::min((int)argb.extract(1), 255);
			*(pixelBuffer + xy+2) = std::min((int)argb.extract(2), 255);
			*(pixelBuffer + xy + 3) = 0;
		}
	};*/
}


// No idea what this should return, but probably a colorX, colorY and colorZ thats why its returning a __m256 array 
AvxVector3 RaySystem::trace(int ind, int depth)
{
	__m256 zero8 = _mm256_setzero_ps();
	AvxVector3 zeroVec = { zero8, zero8, zero8 };
	if (depth > RAYTRACER_RECURSION_DEPTH) return zeroVec;


	__m256 dx = dirX[ind];
	__m256 dy = dirY[ind];
	__m256 dz = dirZ[ind];
	__m256 ox = originX[ind];
	__m256 oy = originY[ind];
	__m256 oz = originZ[ind];
	__m256 len = length[ind];

	// -- hit function  currently not fully AVX --

	HitInfo h[AVX_SIZE];
	for (unsigned int j = 0; j < AVX_SIZE; j++)
	{
		h[j] = { Vec3Df(0),Vec3Df(0),1000 };
		Ray r = { Vec3Df(ox.m256_f32[j], oy.m256_f32[j], oz.m256_f32[j]), Vec3Df(dx.m256_f32[j], dy.m256_f32[j], dz.m256_f32[j]), len.m256_f32[j] };
		for (unsigned int i = 0; i < shapeSize; i++)
		{
			bool earlyOut = shapes[i]->hit(r, &h[j]); 
			if (earlyOut) {
				break;
			}
		}
	}

	HitInfo_Avx hitInfo = HitInfo_Avx();
	hitInfo.nx = _mm256_setr_ps(h[0].normal.get_x(), h[1].normal.get_x(), h[2].normal.get_x(), h[3].normal.get_x(), h[4].normal.get_x(), h[5].normal.get_x(), h[6].normal.get_x(), h[7].normal.get_x());
	hitInfo.ny = _mm256_setr_ps(h[0].normal.get_y(), h[1].normal.get_y(), h[2].normal.get_y(), h[3].normal.get_y(), h[4].normal.get_y(), h[5].normal.get_y(), h[6].normal.get_y(), h[7].normal.get_y());
	hitInfo.nz = _mm256_setr_ps(h[0].normal.get_z(), h[1].normal.get_z(), h[2].normal.get_z(), h[3].normal.get_z(), h[4].normal.get_z(), h[5].normal.get_z(), h[6].normal.get_z(), h[7].normal.get_z());
	hitInfo.px = _mm256_setr_ps(h[0].hitPos.get_x(), h[1].hitPos.get_x(), h[2].hitPos.get_x(), h[3].hitPos.get_x(), h[4].hitPos.get_x(), h[5].hitPos.get_x(), h[6].hitPos.get_x(), h[7].hitPos.get_x());
	hitInfo.py = _mm256_setr_ps(h[0].hitPos.get_y(), h[1].hitPos.get_y(), h[2].hitPos.get_y(), h[3].hitPos.get_y(), h[4].hitPos.get_y(), h[5].hitPos.get_y(), h[6].hitPos.get_y(), h[7].hitPos.get_y());
	hitInfo.pz = _mm256_setr_ps(h[0].hitPos.get_z(), h[1].hitPos.get_z(), h[2].hitPos.get_z(), h[3].hitPos.get_z(), h[4].hitPos.get_z(), h[5].hitPos.get_z(), h[6].hitPos.get_z(), h[7].hitPos.get_z());
	hitInfo.dist = _mm256_setr_ps(h[0].distance, h[1].distance, h[2].distance, h[3].distance, h[4].distance, h[5].distance, h[6].distance, h[7].distance);
	hitInfo.id = _mm256_set_epi32(h[0].id, h[1].id, h[2].id, h[3].id, h[4].id, h[5].id, h[6].id, h[7].id);
	hitInfo.mat.absx = _mm256_setr_ps(h[0].material.absorbtion.get_x(), h[1].material.absorbtion.get_x(), h[2].material.absorbtion.get_x(), h[3].material.absorbtion.get_x(), h[4].material.absorbtion.get_x(), h[5].material.absorbtion.get_x(), h[6].material.absorbtion.get_x(), h[7].material.absorbtion.get_x());
	hitInfo.mat.absy = _mm256_setr_ps(h[0].material.absorbtion.get_y(), h[1].material.absorbtion.get_y(), h[2].material.absorbtion.get_y(), h[3].material.absorbtion.get_y(), h[4].material.absorbtion.get_y(), h[5].material.absorbtion.get_y(), h[6].material.absorbtion.get_y(), h[7].material.absorbtion.get_y());
	hitInfo.mat.absz = _mm256_setr_ps(h[0].material.absorbtion.get_z(), h[1].material.absorbtion.get_z(), h[2].material.absorbtion.get_z(), h[3].material.absorbtion.get_z(), h[4].material.absorbtion.get_z(), h[5].material.absorbtion.get_z(), h[6].material.absorbtion.get_z(), h[7].material.absorbtion.get_z());
	hitInfo.mat.diffx = _mm256_setr_ps(h[0].material.diffuseColor.get_x(), h[1].material.diffuseColor.get_x(), h[2].material.diffuseColor.get_x(), h[3].material.diffuseColor.get_x(), h[4].material.diffuseColor.get_x(), h[5].material.diffuseColor.get_x(), h[6].material.diffuseColor.get_x(), h[7].material.diffuseColor.get_x());
	hitInfo.mat.diffy = _mm256_setr_ps(h[0].material.diffuseColor.get_y(), h[1].material.diffuseColor.get_y(), h[2].material.diffuseColor.get_y(), h[3].material.diffuseColor.get_y(), h[4].material.diffuseColor.get_y(), h[5].material.diffuseColor.get_y(), h[6].material.diffuseColor.get_y(), h[7].material.diffuseColor.get_y());
	hitInfo.mat.diffz = _mm256_setr_ps(h[0].material.diffuseColor.get_z(), h[1].material.diffuseColor.get_z(), h[2].material.diffuseColor.get_z(), h[3].material.diffuseColor.get_z(), h[4].material.diffuseColor.get_z(), h[5].material.diffuseColor.get_z(), h[6].material.diffuseColor.get_z(), h[7].material.diffuseColor.get_z());
	hitInfo.mat.specx = _mm256_setr_ps(h[0].material.specularColor.get_x(), h[1].material.specularColor.get_x(), h[2].material.specularColor.get_x(), h[3].material.specularColor.get_x(), h[4].material.specularColor.get_x(), h[5].material.specularColor.get_x(), h[6].material.specularColor.get_x(), h[7].material.specularColor.get_x());
	hitInfo.mat.specy = _mm256_setr_ps(h[0].material.specularColor.get_y(), h[1].material.specularColor.get_y(), h[2].material.specularColor.get_y(), h[3].material.specularColor.get_y(), h[4].material.specularColor.get_y(), h[5].material.specularColor.get_y(), h[6].material.specularColor.get_y(), h[7].material.specularColor.get_y());
	hitInfo.mat.specz = _mm256_setr_ps(h[0].material.specularColor.get_z(), h[1].material.specularColor.get_z(), h[2].material.specularColor.get_z(), h[3].material.specularColor.get_z(), h[4].material.specularColor.get_z(), h[5].material.specularColor.get_z(), h[6].material.specularColor.get_z(), h[7].material.specularColor.get_z());
	hitInfo.mat.ambx = _mm256_setr_ps(h[0].material.ambientColor.get_x(), h[1].material.ambientColor.get_x(), h[2].material.ambientColor.get_x(), h[3].material.ambientColor.get_x(), h[4].material.ambientColor.get_x(), h[5].material.ambientColor.get_x(), h[6].material.ambientColor.get_x(), h[7].material.ambientColor.get_x());
	hitInfo.mat.amby = _mm256_setr_ps(h[0].material.ambientColor.get_y(), h[1].material.ambientColor.get_y(), h[2].material.ambientColor.get_y(), h[3].material.ambientColor.get_y(), h[4].material.ambientColor.get_y(), h[5].material.ambientColor.get_y(), h[6].material.ambientColor.get_y(), h[7].material.ambientColor.get_y());
	hitInfo.mat.ambz = _mm256_setr_ps(h[0].material.ambientColor.get_z(), h[1].material.ambientColor.get_z(), h[2].material.ambientColor.get_z(), h[3].material.ambientColor.get_z(), h[4].material.ambientColor.get_z(), h[5].material.ambientColor.get_z(), h[6].material.ambientColor.get_z(), h[7].material.ambientColor.get_z());
	hitInfo.mat.mirror = _mm256_setr_ps(h[0].material.mirror, h[1].material.mirror, h[2].material.mirror, h[3].material.mirror, h[4].material.mirror, h[5].material.mirror, h[6].material.mirror, h[7].material.mirror);
	hitInfo.mat.refracIndex = _mm256_setr_ps(h[0].material.refracIndex, h[1].material.refracIndex, h[2].material.refracIndex, h[3].material.refracIndex, h[4].material.refracIndex, h[5].material.refracIndex, h[6].material.refracIndex, h[7].material.refracIndex);
	
	// end of hit function

	__m256 distMask = _mm256_cmp_ps(hitInfo.dist, _mm256_set1_ps(RAYTRACER_MAX_RENDERDISTANCE), _CMP_LE_OS);
	__m256 one8 = _mm256_set1_ps(1);
	__m256 matRefracIndex = hitInfo.mat.refracIndex;
	__m256 refracMask = _mm256_cmp_ps(matRefracIndex, one8, _CMP_GT_OS);

	AvxVector3 rayDir = { dx, dy, dz };
	AvxVector3 hitNormal = { hitInfo.nx, hitInfo.ny, hitInfo.nz };
	__m256 dotDirNor = dot_product(rayDir, hitNormal);
	__m256 condMask = _mm256_cmp_ps(dotDirNor, zero8, _CMP_LT_OS);

	__m256 refracIndex = _mm256_blendv_ps(_mm256_mul_ps(matRefracIndex, matRefracIndex), one8, condMask);
	__m256 cosTheta = _mm256_div_ps(_mm256_sub_ps(one8, _mm256_mul_ps(_mm256_set1_ps(1.000586f), _mm256_sub_ps(_mm256_set1_ps(1), _mm256_mul_ps(dotDirNor, dotDirNor)))), _mm256_mul_ps(refracIndex, refracIndex));
	__m256 refractedMask = _mm256_cmp_ps(cosTheta, zero8, _CMP_GT_OS);
	AvxVector3 sinPhi = mul(sub(rayDir, mul(hitNormal, dotDirNor)), refracIndex);
	AvxVector3 refDir = normalize(sub(sinPhi, mul(hitNormal, _mm256_sqrt_ps(cosTheta))));
	AvxVector3 refractDir = blend(zeroVec, refDir, refractedMask);
	AvxVector3 negMatAbs = { -hitInfo.mat.absx, -hitInfo.mat.absy, -hitInfo.mat.absz };
	AvxVector3 power = mul(negMatAbs, refractDir);
	__m256 e8 = _mm256_set1_ps(E);
	AvxVector3 kp8 = { _mm256_pow_ps(e8, power.x), _mm256_pow_ps(e8, power.y), _mm256_pow_ps(e8, power.z) };
	AvxVector3 k = blend(zeroVec, kp8, _mm256_andnot_ps(refracMask, condMask));

	__m256 R0 = _mm256_div_ps(_mm256_mul_ps(_mm256_sub_ps(matRefracIndex, one8), _mm256_sub_ps(matRefracIndex, one8)), _mm256_mul_ps(_mm256_add_ps(matRefracIndex, one8), _mm256_add_ps(matRefracIndex, one8)));
	__m256 c_comp = _mm256_sub_ps(one8, dotDirNor);
	__m256 R = _mm256_add_ps(R0, _mm256_mul_ps(_mm256_sub_ps(one8, R0), _mm256_pow_ps(c_comp, _mm256_set1_ps(5))));

	// mirror ray
	AvxVector3 mirrDir = normalize(sub(rayDir, mul(hitNormal, dotDirNor * 2)));
	AvxVector3 hitPos = { hitInfo.px, hitInfo.py, hitInfo.pz };
	__m256 migraine8 = _mm256_set1_ps(RAY_MIGRAINE);
	AvxVector3 newOriginM = add(hitPos, mul(mirrDir, migraine8));
	originX[ind] = newOriginM.x;
	originY[ind] = newOriginM.y;
	originZ[ind] = newOriginM.z;
	dirX[ind] = mirrDir.x;
	dirY[ind] = mirrDir.y;
	dirZ[ind] = mirrDir.z;
	length[ind] = _mm256_set1_ps(1000);
	AvxVector3 mCol = zeroVec;// trace(ind, depth + 1);

	// refracted ray
	AvxVector3 newOriginR = add(hitPos, mul(refractDir, migraine8));
	originX[ind] = newOriginR.x;
	originY[ind] = newOriginR.y;
	originZ[ind] = newOriginR.z;
	dirX[ind] = refractDir.x;
	dirY[ind] = refractDir.y;
	dirZ[ind] = refractDir.z;
	length[ind] = _mm256_set1_ps(1000);
	AvxVector3 rCol = zeroVec;// trace(ind, depth + 1);

	// two rays bounced on one hit
	AvxVector3 color1 = add(mul(mul(k, R), mCol), mul(rCol, _mm256_sub_ps(one8, R)));

	__m256 colx = hitInfo.mat.ambx;
	__m256 coly = hitInfo.mat.amby;
	__m256 colz = hitInfo.mat.ambz;

	AvxVector3 diffCol = { hitInfo.mat.diffx, hitInfo.mat.diffy, hitInfo.mat.diffz };
	AvxVector3 specCol = { hitInfo.mat.specx, hitInfo.mat.specy, hitInfo.mat.specz };
	for (Light* l : lights)
	{
		AvxVector3 lPos = { _mm256_set1_ps(l->position.get_x()), _mm256_set1_ps(l->position.get_y()), _mm256_set1_ps(l->position.get_z()) };
		AvxVector3 lightDist = sub(lPos, hitPos);
		__m256 lightMask = _mm256_cmp_ps(dot_product(hitNormal, lightDist), zero8, _CMP_LT_OS);

		AvxVector3 lightV = normalize(lightDist);
		__m256 lightDistAbs = vector_length(lightDist);
		__m256 shadowLength = _mm256_sub_ps(lightDistAbs, _mm256_mul_ps(migraine8, _mm256_set1_ps(2)));

		AvxVector3 halfVector = normalize(sub(lightV, rayDir));
		AvxVector3 lightInt = { _mm256_set1_ps(l->intensity.get_x()), _mm256_set1_ps(l->intensity.get_y()), _mm256_set1_ps(l->intensity.get_z()) };
		AvxVector3 diffuse = mul(lightInt, _mm256_rcp_ps(lightDistAbs));

		AvxVector3 blinnphong = mul(mul(diffCol, lightInt), _mm256_max_ps(zero8, dot_product(hitNormal, lightV)));
		__m256 specCoef = _mm256_pow_ps(_mm256_max_ps(zero8, dot_product(hitNormal, halfVector)), _mm256_set1_ps(BLINN_PHONG_POWER));
		AvxVector3 specular = mul(mul(diffCol, specCol), mul(lightInt, specCoef));
		
		AvxVector3 lightColor = mul(diffuse, add(blinnphong, specular));
		colx = _mm256_add_ps(colx, lightColor.x);
		coly = _mm256_add_ps(coly, lightColor.y);
		colx = _mm256_add_ps(colz, lightColor.z);
	}

	AvxVector3 calcLight = { colz, coly, colx };
	AvxVector3 color2 = mul(calcLight, diffCol);
	r[ind] = _mm256_and_ps(_mm256_blendv_ps(color2.x, color1.x, refracMask), distMask);
	g[ind] = _mm256_and_ps(_mm256_blendv_ps(color2.y, color1.y, refracMask), distMask);
	b[ind] = _mm256_and_ps(_mm256_blendv_ps(color2.z, color1.z, refracMask), distMask);

	return { r[ind], g[ind], b[ind] };

	//if (hitInfo.material.refracIndex > 1)
	//{
	//	// Init beers law
	//	Vec3Df k = Vec3Df(1);
	//	float c = 0;
	//	lastId = hitInfo.id;

	//	bool cond = dot_product(ray.direction, hitInfo.normal) < 0;
	//	Vec3Df hitNormal = hitInfo.normal * (cond ? 1 : -1);
	//	float refracIndex = cond ? hitInfo.material.refracIndex : (1 / hitInfo.material.refracIndex);
	//	// Obtuse angle
	//	float cosTheta = rayCosTheta(ray, hitNormal, refracIndex);
	//	bool refracted = cosTheta >= 0;
	//	Vec3Df refractDir = refracted ? refractRay(ray, hitNormal, refracIndex, cosTheta) : Vec3Df(0);
	//	if (!cond)
	//	{
	//		if (!refracted)
	//		{
	//			return trace(mirrorRay(ray.direction, hitInfo), depth + 1);
	//		}
	//		Vec3Df power = -hitInfo.material.absorbtion * refractDir;
	//		k = Vec3Df(pow(E, power[0]), pow(E, power[1]), pow(E, power[2]));
	//	}
	//	Ray refractedRay = { hitInfo.hitPos + refractDir * Vec3Df(RAY_MIGRAINE), refractDir, 1000 };
	//	Vec3Df rayDir = cond ? -ray.direction : ray.direction;
	//	c = dot_product(rayDir, hitInfo.normal);
	//	float R0 = ((hitInfo.material.refracIndex - 1) * (hitInfo.material.refracIndex - 1)) / ((hitInfo.material.refracIndex + 1) * (hitInfo.material.refracIndex + 1));
	//	float c_comp = 1 - c;
	//	float R = R0 + (1 - R0) * c_comp * c_comp * c_comp * c_comp * c_comp;
	//	return  k * (R * trace(mirrorRay(ray.direction, hitInfo), depth + 1)) + (1 - R) * trace(refractedRay, depth + 1);
	//}
	//else
	//{
	//	Vec3Df normalCol = calculateLight(hitInfo, ray.direction) * hitInfo.material.diffuseColor;
	//	if (hitInfo.material.mirror == 0)
	//	{
	//		return normalCol;
	//	}
	//	else
	//	{
	//		Vec3Df mirrCol = hitInfo.material.diffuseColor * trace(mirrorRay(ray.direction, hitInfo), depth + 1);
	//		Vec3Df finalCol = mirrCol * Vec3Df(hitInfo.material.mirror) + normalCol * (Vec3Df(1 - hitInfo.material.mirror));
	//		return finalCol;
	//	}
	//}
}

// rewrite this later
//void RaySystem::hit(int ind)
//{
//	__m256 dx = dirX[ind];
//	__m256 dy = dirY[ind];
//	__m256 dz = dirZ[ind];
//	__m256 ox = originX[ind];
//	__m256 oy = originY[ind];
//	__m256 oz = originZ[ind];
//	__m256 len = length[ind];
//
//	
//
//
//} 