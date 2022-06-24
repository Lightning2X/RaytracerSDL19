#include "precomp.h"

RaytracerRenderer::RaytracerRenderer(Screen* screen) : Renderer(screen)
{
	width = screen->getWidth();
	height = screen->getHeight();
}

RaytracerRenderer::~RaytracerRenderer()
{
}

void RaytracerRenderer::draw(int iteration)
{
	assert(currentScene != NULL && camera != NULL);
	shapes = currentScene->objects;
	shapeSize = shapes.size();
	lights = currentScene->lights;
	lightSize = lights.size();
	Vec3Df rayStartDir = camera->getTopLeft();
	Vec3Df xOffset = Vec3Df( (camera->getTopRight() - camera->getTopLeft()) / (width - 1));
	Vec3Df yOffset = Vec3Df((camera->getBottomLeft() - camera->getTopLeft()) / (height - 1));
	#pragma unroll
	for (unsigned int y = 0; y < height; y++) 
	{
		for (unsigned int x = 0; x < width; x+=2)
		{

			// Pixel 1.....
			// Make ray
			Vec3Df direction = rayStartDir + Vec3Df(x)* xOffset  + Vec3Df(y)*yOffset;
			direction -= camera->position;
			direction = normalize_vector(direction);
			lastId = -1;
			Ray r = { camera->position, direction, 100,1.000293f, 1.000586f };
			// Check for hits
			Vec3Df argb = trace(r,0) * Vec3Df(255);

			// Convert color
			unsigned int xy = x*4 + (y * width)*4;
			*(pixelBuffer + xy) = std::min((int)argb.extract(0), 255);
			*(pixelBuffer + xy+1) = std::min((int)argb.extract(1), 255);
			*(pixelBuffer + xy+2) = std::min((int)argb.extract(2), 255);
			*(pixelBuffer + xy+3) = 0;


			// Pixel 2....
			// Make ray
			direction = rayStartDir + Vec3Df(x+1) * xOffset + Vec3Df(y) * yOffset;
			direction -= camera->position;
			direction = normalize_vector(direction);
			lastId = -1;
			r = { camera->position, direction, 100,1.000293f, 1.000586f };
			// Check for hits
			argb = trace(r, 0) * Vec3Df(255);

			// Pixel 1
			// Convert color
			xy = x * 4 + (y * width) * 4;
			*(pixelBuffer + xy+4) = std::min((int)argb.extract(0),255);
			*(pixelBuffer + xy + 5) = std::min((int)argb.extract(1),255);
			*(pixelBuffer + xy + 6) = std::min((int)argb.extract(2), 255);
			*(pixelBuffer + xy + 7) = 0;

		}
	}
}

Vec3Df RaytracerRenderer::trace(Ray ray, int depth)
{
	if (depth > RAYTRACER_RECURSION_DEPTH)
		return Vec3Df(0);
	HitInfo hitInfo = { Vec3Df(0),Vec3Df(0),1000 };
	for (size_t i = 0; i < shapeSize; i++)
	{
		shapes[i]->hit(ray, &hitInfo);
	}
	// Not a hit, so return
	if (hitInfo.distance > RAYTRACER_MAX_RENDERDISTANCE)
	{
		return Vec3Df(0);
	}
	if (hitInfo.material.mirror == 0 && hitInfo.material.refracIndex <= 1)
	{
		return calculateLight(hitInfo, ray.direction)*hitInfo.material.diffuseColor;
	}
	else if(hitInfo.material.refracIndex <= 1)
	{
		Vec3Df normalCol = calculateLight(hitInfo, ray.direction) * hitInfo.material.diffuseColor;
		Vec3Df mirrCol = hitInfo.material.diffuseColor*trace(mirrorRay(ray.direction, hitInfo), depth + 1);
		Vec3Df finalCol = mirrCol * Vec3Df(hitInfo.material.mirror) + normalCol * (Vec3Df(1 - hitInfo.material.mirror));
		return finalCol;
	}
	else
	{
		// Init beers law
		Vec3Df k = Vec3Df(1);
		float c = 0;
		lastId = hitInfo.id;

		// Obtuse angle
		if (dot_product(ray.direction, hitInfo.normal) < 0)
		{
			float cosTheta = rayCosTheta(ray, hitInfo.normal, hitInfo.material.refracIndex);
			bool refracted = cosTheta >= 0;
			Vec3Df refractDir =  refracted ? refractRay(ray, hitInfo.normal, hitInfo.material.refracIndex, cosTheta) : Vec3Df(0);
			Ray refractedRay = { hitInfo.hitPos + refractDir * Vec3Df(RAY_MIGRAINE), refractDir, 1000 };
			c = dot_product(-ray.direction, hitInfo.normal);
			// Calculate the reflectance at normal incidence.
			float R0 = ((hitInfo.material.refracIndex - 1) * (hitInfo.material.refracIndex - 1)) / ((hitInfo.material.refracIndex + 1) * (hitInfo.material.refracIndex + 1));
			// Calculate schlick's approximation (amount of % the refraction contributes to the total).
			float R = R0 + (1 - R0) * pow((1 - c), 5);
			// The ray has now hit something refractive, if two spheres border each other we want them to refract off each other's indices and not the indice of air, and thus we set the last hit refract indice for the ray.
			if (lastId == hitInfo.id)
			{
				ray.refracIndex = 1.000293f;
				ray.refracIndex2 = 1.000586f;
			}
			else
			{
				ray.refracIndex = hitInfo.material.refracIndex;
				ray.refracIndex2 = hitInfo.material.refracIndex * hitInfo.material.refracIndex;
			}
			// Return percentage of mirror and refraction that results from the above calculations, where R is the amount reflected and 1-R the amount refracted.
			return  k * (R * trace(mirrorRay(ray.direction, hitInfo), depth + 1)) + (1 - R) * trace(refractedRay, depth + 1);
		}
		else
		{
			float cosTheta = rayCosTheta(ray, -hitInfo.normal, 1/hitInfo.material.refracIndex);
			bool refracted = cosTheta >= 0;
			Vec3Df refractDir = refracted ? refractRay(ray, -hitInfo.normal, 1/hitInfo.material.refracIndex, cosTheta) : Vec3Df(0);
			Vec3Df power = -hitInfo.material.absorbtion * refractDir;
			k = Vec3Df(pow(E, power[0]), pow(E, power[1]), pow(E, power[2]));
			if (!refracted)
			{
				return k * trace(mirrorRay(ray.direction, hitInfo), depth + 1);
			}
			Ray refractedRay = { hitInfo.hitPos + refractDir * Vec3Df(RAY_MIGRAINE), refractDir, 1000 };
			c = dot_product(ray.direction, hitInfo.normal);
			// Calculate the reflectance at normal incidence.
			float R0 = ((hitInfo.material.refracIndex - 1) * (hitInfo.material.refracIndex - 1)) / ((hitInfo.material.refracIndex + 1) * (hitInfo.material.refracIndex + 1));
			// Calculate schlick's approximation (amount of % the refraction contributes to the total).
			float R = R0 + (1 - R0) * pow((1 - c), 5);
			// The ray has now hit something refractive, if two spheres border each other we want them to refract off each other's indices and not the indice of air, and thus we set the last hit refract indice for the ray.
			if (lastId == hitInfo.id)
			{
				ray.refracIndex = 1.000293f;
				ray.refracIndex2 = 1.000586f;
			}
			else
			{
				ray.refracIndex = hitInfo.material.refracIndex;
				ray.refracIndex2 = hitInfo.material.refracIndex* hitInfo.material.refracIndex;
			}
			// Return percentage of mirror and refraction that results from the above calculations, where R is the amount reflected and 1-R the amount refracted.
			return  k * (R * trace(mirrorRay(ray.direction, hitInfo), depth + 1)) + (1 - R) * trace(refractedRay, depth + 1);
		}
	}
}

Vec3Df RaytracerRenderer::calculateLight(HitInfo hitI, Vec3Df direction)
{
	Vec3Df argb = hitI.material.ambientColor;
	for (Light* l : lights)
	{
		Vec3Df lightDist = l->position - hitI.hitPos;
		Vec3Df lightV = normalize_vector(lightDist);
		//If we cannot reach this light from the intersection point we go on to the next light.
		if (dot_product(hitI.normal, lightDist) < 0)
			continue;
		// blocked
		Vec3Df shadowOrigin = hitI.hitPos + lightV * Vec3Df(RAY_MIGRAINE);
		float shadowLength = (float)(vector_length(lightDist) - 2 * RAY_MIGRAINE);
		Ray shadowR = { shadowOrigin, lightV, shadowLength };
		for (size_t i = 0; i < shapeSize; i++)
		{
			if (shapes[i]->fastHit(shadowR))
				continue;
		}
		// Diffuse
		Vec3Df halfVector = normalize_vector(lightV - direction);
		Vec3Df diffuse = l->intensity / vector_length(lightDist);
		// Blinn phong
		Vec3Df blinnphong = hitI.material.diffuseColor * l->intensity * Vec3Df(std::max(0.0f, dot_product(hitI.normal, lightV)));
		Vec3Df specular = hitI.material.diffuseColor * hitI.material.specularColor * l->intensity;
		specular *= Vec3Df( (float)pow(std::max(0.0f, dot_product(hitI.normal, halfVector)), BLINN_PHONG_POWER) );
		blinnphong += specular;
		Vec3Df lightColor = diffuse * blinnphong;
		argb += lightColor;
	}
	return argb;
}

Ray RaytracerRenderer::mirrorRay(Vec3Df originalDir, HitInfo hitSurface)
{
	Vec3Df mirrDir = originalDir - (hitSurface.normal * Vec3Df((2.0f * dot_product(originalDir, hitSurface.normal))));
	mirrDir = normalize_vector(mirrDir);
	Ray mirrorRay = Ray{ hitSurface.hitPos + Vec3Df(RAY_MIGRAINE) * mirrDir, mirrDir, 1000,1.000293f, 1.000586f };
	return mirrorRay;
}

float RaytracerRenderer::rayCosTheta(Ray r, Vec3Df normal, float rIndex)
{
	float dotDirNor = dot_product(r.direction, normal);
	float cosVal = (1.0f - (r.refracIndex2 * (1.0f - (dotDirNor* dotDirNor))) / (rIndex * rIndex));
	return cosVal;

}

Vec3Df RaytracerRenderer::refractRay(Ray r, Vec3Df normal, float rIndex, float rayCosTheta)
{
	// Calculate the sine of the phi angle of the transmission vector t.
	Vec3Df sinPhi = (r.refracIndex * (r.direction - normal * dot_product(r.direction, normal))) / rIndex;
	// Calculate the finale refraction direction
	Vec3Df refractDir = normalize_vector(sinPhi - normal * Vec3Df(sqrtf(rayCosTheta)));
	return refractDir;
}
