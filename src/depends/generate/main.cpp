// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "../../GClasses/GApp.h"
#include "../../GClasses/GBits.h"
#include "../../GClasses/GError.h"
#include "../../GClasses/GHeap.h"
#include "../../GClasses/GImage.h"
#include "../../GClasses/GRand.h"
#include "../../GClasses/GFile.h"
#include "../../GClasses/GFunction.h"
#include "../../GClasses/GHistogram.h"
#include "../../GClasses/GTransform.h"
#include "../../GClasses/GVec.h"
#include "../../GClasses/GSparseMatrix.h"
#include "../../GClasses/GHashTable.h"
#include "../../GClasses/GHillClimber.h"
#include "../../GClasses/GManifold.h"
#include "../../GClasses/GMath.h"
#include "../../GClasses/GMatrix.h"
#include "../../GClasses/GNeuralNet.h"
#include "../../GClasses/GPlot.h"
#include "../../GClasses/GRayTrace.h"
#include "../../GClasses/GRect.h"
#include "../../GClasses/GSystemLearner.h"
#include "../../GClasses/GDom.h"
#include "../../wizard/usage.h"
#include <time.h>
#include <iostream>
#include <sstream>
#ifdef WIN32
#	include <direct.h>
#	include <process.h>
#endif
#include <exception>
#include <string>
#include "GImagePng.h"

using namespace GClasses;
using std::cout;
using std::cerr;
using std::vector;
using std::string;
using std::ostringstream;

GMatrix* loadData(const char* szFilename)
{
	// Load the dataset by extension
	PathData pd;
	GFile::parsePath(szFilename, &pd);
	GMatrix* pData = NULL;
	if(_stricmp(szFilename + pd.extStart, ".arff") == 0)
		pData = GMatrix::loadArff(szFilename);
	else if(_stricmp(szFilename + pd.extStart, ".csv") == 0)
		pData = GMatrix::loadCsv(szFilename, ',', false, false);
	else if(_stricmp(szFilename + pd.extStart, ".dat") == 0)
		pData = GMatrix::loadCsv(szFilename, '\0', false, false);
	else
		throw Ex("Unsupported file format: ", szFilename + pd.extStart);
	return pData;
}

void cube(GArgReader& args)
{
	int side = args.pop_uint() - 1;
	GMatrix data(0, 3);
	double* pRow;
	for(int y = 0; y < side; y++)
	{
		double b = ((double)y/* + 0.5*/) / side;
		for(int x = 0; x < side; x++)
		{
			double a = ((double)x/* + 0.5*/) / side;
			pRow = data.newRow(); pRow[0] = 0.0; pRow[1] = 1.0 - a; pRow[2] = b;
			pRow = data.newRow(); pRow[0] = 1.0; pRow[1] = a; pRow[2] = 1.0 - b;
			pRow = data.newRow(); pRow[0] = b; pRow[1] = 0.0; pRow[2] = 1.0 - a;
			pRow = data.newRow(); pRow[0] = 1.0 - b; pRow[1] = 1.0; pRow[2] = a;
			pRow = data.newRow(); pRow[0] = 1.0 - a; pRow[1] = b; pRow[2] = 0.0;
			pRow = data.newRow(); pRow[0] = a; pRow[1] = 1.0 - b; pRow[2] = 1.0;
		}
	}
	pRow = data.newRow(); pRow[0] = 0.0; pRow[1] = 0.0; pRow[2] = 0.0;
	pRow = data.newRow(); pRow[0] = 1.0; pRow[1] = 1.0; pRow[2] = 1.0;
	data.print(cout);
}

void fishBowl(GArgReader& args)
{
	int points = args.pop_uint();

	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	double opening = 0.25;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-opening"))
			opening = args.pop_double();
	}
	if(opening >= 2.0)
		throw Ex("opening too big--consumes entire fish bowl");

	// Make the data
	GRand prng(seed);
	GMatrix data(0, 3);
	for(int i = 0; i < points; i++)
	{
		double* pRow = data.newRow();
		int j;
		for(j = 100; j > 0; j--)
		{
			prng.spherical(pRow, 3);
			if(pRow[1] < 1.0 - opening)
				break;
		}
		if(j == 0)
			throw Ex("Failed to find a point on the fish bowl");
	}
	data.sort(1);
	data.print(cout);
}

void Noise(GArgReader& args)
{
	int pats = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	string dist = "gaussian";
	double p1 = 0.0;
	double p2 = 1.0;
	vector<double> probs;
	int dims = 1;
	int vals = 0;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-dist"))
		{
			dist = args.pop_string();
			if(dist.compare("gaussian") == 0 || dist.compare("normal") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("uniform") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("beta") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("binomial") == 0)
			{
				p1 = (double)args.pop_uint();
				p2 = args.pop_double();
			}
			else if(dist.compare("categorical") == 0)
			{
				vals = args.pop_uint();
				for(int i = 0; i < vals; i++)
					probs.push_back(args.pop_double());
			}
			else if(dist.compare("cauchy") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("chisquare") == 0)
				p1 = args.pop_double();
			else if(dist.compare("exponential") == 0)
				p1 = args.pop_double();
			else if(dist.compare("f") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("gamma") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("geometric") == 0)
				p1 = args.pop_double();
			else if(dist.compare("logistic") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("lognormal") == 0)
			{
				p1 = args.pop_double();
				p2 = args.pop_double();
			}
			else if(dist.compare("poisson") == 0)
				p1 = args.pop_double();
			else if(dist.compare("softimpulse") == 0)
				p1 = args.pop_double();
			else if(dist.compare("spherical") == 0)
			{
				dims = args.pop_uint();
				p1 = args.pop_double();
			}
			else if(dist.compare("student") == 0)
				p1 = args.pop_double();
			else if(dist.compare("weibull") == 0)
				p1 = args.pop_double();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the data
	GRand prng(nSeed);
	GUniformRelation* pRelation = new GUniformRelation(dims, vals);
	sp_relation pRel = pRelation;
	GMatrix data(pRel);
	if(dist.compare("gaussian") == 0 || dist.compare("normal") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.normal() * p2 + p1;
	}
	else if(dist.compare("uniform") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.uniform() * (p2 - p1) + p1;
	}
	else if(dist.compare("beta") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.beta(p1, p2);
	}
	else if(dist.compare("binomial") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.binomial((int)p1, p2);
	}
	else if(dist.compare("categorical") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = (double)prng.categorical(probs);
	}
	else if(dist.compare("cauchy") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.cauchy() * p2 + p1;
	}
	else if(dist.compare("chisquare") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.chiSquare(p1);
	}
	else if(dist.compare("exponential") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.exponential() * p1;
	}
	else if(dist.compare("f") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.f(p1, p2);
	}
	else if(dist.compare("gamma") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.gamma(p1) / p2;
	}
	else if(dist.compare("geometric") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = (double)prng.geometric(p1);
	}
	else if(dist.compare("logistic") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.logistic() * p2 + p1;
	}
	else if(dist.compare("lognormal") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.logNormal(p1, p2);
	}
	else if(dist.compare("poisson") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = (double)prng.poisson(p1);
	}
	else if(dist.compare("softimpulse") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.softImpulse(p1);
	}
	else if(dist.compare("spherical") == 0)
	{
		for(int i = 0; i < pats; i++)
		{
			double* pRow = data.newRow();
			prng.spherical(pRow, dims);
			GVec::multiply(pRow, p1, dims);
		}
	}
	else if(dist.compare("student") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.student(p1);
	}
	else if(dist.compare("weibull") == 0)
	{
		for(int i = 0; i < pats; i++)
			data.newRow()[0] = prng.weibull(p1);
	}
	else
		throw Ex("Unrecognized distribution: ", dist.c_str());

	data.print(cout);
}

bool isInsideUnitStar(double x, double y)
{
	int count = 0;
	for(int n = 0; n < 5; n++)
	{
		double r = n * 2 * M_PI / 5;
		double c = cos(r);
		double s = sin(r);
		x += s;
		y -= c;
		if((x * s) - (y * c) >= 0)
			count++;
		x -= s;
		y += c;
	}
	return (count >= 4);
}

double LengthOfSwissRoll(double x)
{
#ifdef WIN32
	GAssert(false); // not implemented yet for Win32
	return 0;
#else
	return (x * sqrt(x * x + 1) + asinh(x)) / 2;
#endif
}

void SwissRoll(GArgReader& args)
{
	int points = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	bool cutOutStar = false;
	bool reduced = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-cutoutstar"))
			cutOutStar = true;
		else if(args.if_pop("-reduced"))
			reduced = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Generate the data
	double unrolledWidth = 48.790603865663797;
	double starCenterX = 43;
	double starSize = 1.5;
	GRand prng(nSeed);
	GArffRelation rel;
	rel.addAttribute("x", 0, NULL);
	rel.addAttribute("y", 0, NULL);
	if(!reduced)
		rel.addAttribute("z", 0, NULL);
	GMatrix data(0, 3);
	for(int n = 0; n < points; n++)
	{
		double t = ((double)n * 8) / points;
		double* pVector = data.newRow();
		pVector[0] = ((t + 2) * sin(t));
		pVector[2] = ((t + 2) * cos(t));
		if(cutOutStar)
		{
			int i;
			for(i = 0; i < 1000; i++)
			{
				pVector[1] = prng.uniform() * 12;
				if(!isInsideUnitStar((n * unrolledWidth / points - starCenterX) / starSize, (6 - pVector[1]) / starSize))
					break;
			}
			if(i >= 1000)
				throw Ex("The star is too big. It severs the manifold.");
		}
		else
			pVector[1] = prng.uniform() * 12;
		if(reduced)
		{
			pVector[0] = pVector[1];
			pVector[1] = LengthOfSwissRoll(t + 2);
			pVector[2] = 0;
		}
	}

	// Print the data
	data.print(cout);
}

double LengthOfSineFunc(void* pThis, double x)
{
	double d = cos(x);
	return sqrt(d * d + 1.0);
}

void SCurve(GArgReader& args)
{
	int points = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	bool reduced = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-reduced"))
			reduced = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Generate the data
	GRand prng(nSeed);
	GArffRelation rel;
	rel.addAttribute("x", 0, NULL);
	rel.addAttribute("y", 0, NULL);
	if(!reduced)
		rel.addAttribute("z", 0, NULL);
	GMatrix data(0, 3);
	for(int n = 0; n < points; n++)
	{
		double t = ((double)n * 2.2 * M_PI - .1 * M_PI) / points;
		double* pVector = data.newRow();
		pVector[0] = 1.0 - sin(t);
		pVector[1] = t;
		pVector[2] = prng.uniform() * 2;
		if(reduced)
		{
			pVector[0] = pVector[2];
			pVector[1] = (n > 0 ? GMath::integrate(LengthOfSineFunc, 0, t, n + 30, NULL) : 0);
			pVector[2] = 0;
		}
	}

	// Print the data
	data.print(cout);
}

void EntwinedSpirals(GArgReader& args)
{
	int points = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	bool reduced = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-reduced"))
			reduced = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Generate the data
	GRand prng(nSeed);
	GArffRelation rel;
	rel.addAttribute("x", 0, NULL);
	if(!reduced)
	{
		rel.addAttribute("y", 0, NULL);
		rel.addAttribute("z", 0, NULL);
	}
	GMatrix data(0, 3);

	double dHeight = 3;
	double dWraps = 1.5;
	double dSpiralLength = sqrt((dWraps * 2.0 * M_PI) * (dWraps * 2.0 * M_PI) + dHeight * dHeight);
	double dTotalLength = 2.0 * (dSpiralLength + 1); // radius = 1
	double d;
	for(int n = 0; n < points; n++)
	{
		double t = ((double)n * dTotalLength) / points;
		double* pVector = data.newRow();
		if(reduced)
		{
			pVector[0] = t;
			pVector[1] = 0;
			pVector[2] = 0;
		}
		else
		{
			if(t < dSpiralLength)
			{
				d = (dSpiralLength - t) * dWraps * 2 * M_PI / dSpiralLength; // d = radians
				pVector[0] = -cos(d);
				pVector[1] = dHeight * t / dSpiralLength;
				pVector[2] = -sin(d);
			}
			else if(t - 2.0 - dSpiralLength >= 0)
			{
				d = (t - 2.0 - dSpiralLength) * dWraps * 2 * M_PI / dSpiralLength; // d = radians
				pVector[0] = cos(d);
				pVector[1] = dHeight * (dSpiralLength - (t - 2.0 - dSpiralLength)) / dSpiralLength;
				pVector[2] = sin(d);
			}
			else
			{
				d = (t - dSpiralLength) / 2.0; // 2 = diameter
				pVector[0] = 2.0 * d - 1.0;
				pVector[1] = dHeight;
				pVector[2] = 0;
			}
		}
	}

	// Print the data
	data.print(cout);
}

void ImageTranslatedOverNoise(GArgReader& args)
{
	const char* szFilenameIn = args.pop_string();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	bool reduced = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-reduced"))
			reduced = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Load the image
	GImage imageFace;
	loadPng(&imageFace, szFilenameIn);

	// Generate the noise image
	GRand prng(nSeed);
	GImage imageNoise;
	imageNoise.setSize(imageFace.width() * 2, imageFace.height() * 2);
	for(int y = 0; y < (int)imageNoise.height(); y++)
	{
		for(int x = 0; x < (int)imageNoise.width(); x++)
			imageNoise.setPixel(x, y, gARGB(0xff, (int)prng.next(256), (int)prng.next(256), (int)prng.next(256)));
	}

	// Make the relation
	GImage imageAll;
	imageAll.setSize(imageNoise.width(), imageNoise.height());
	GArffRelation rel;
	if(reduced)
	{
		rel.addAttribute("x", 0, NULL);
		rel.addAttribute("y", 0, NULL);
	}
	else
	{
		for(int y = 0; y < (int)imageAll.height(); y++)
		{
			for(int x = 0; x < (int)imageAll.width(); x++)
				rel.addAttribute("pix", 0, NULL);
		}
	}

	// Generate the data
	GMatrix data(0, reduced ? 2 : imageAll.width() * imageAll.height());
	GRect r(0, 0, imageFace.width(), imageFace.height());
	for(int y = 0; y <= (int)imageFace.height(); y++)
	{
		for(int x = 0; x <= (int)imageFace.width(); x++)
		{
			if(reduced)
			{
				double* pVec = data.newRow();
				pVec[0] = x;
				pVec[1] = y;
			}
			else
			{
				imageAll.copy(&imageNoise);
				imageAll.blit(x, y, &imageFace, &r);
				double* pVec = data.newRow();
				for(int yy = 0; yy < (int)imageAll.height(); yy++)
				{
					for(int xx = 0; xx < (int)imageAll.width(); xx++)
						pVec[imageAll.width() * yy + xx] = (double)gGray(imageAll.pixel(xx, yy)) / MAX_GRAY_VALUE;
				}
			}
		}
	}

	// Print the data
	data.print(cout);
}

void SelfIntersectingRibbon(GArgReader& args)
{
	unsigned int points = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	GRand prng(nSeed);
	GMatrix data(0, 3);
	for(unsigned int i = 0; i < points; i++)
	{
		double t = M_PI / 4 + (1.5 * M_PI) * (double)i / points;
		double* pPat = data.newRow();
		pPat[0] = sin(t * 2);
		pPat[1] = -2.0 * cos(t);
		pPat[2] = 2.0 * prng.uniform();
	}

	data.print(cout);
}

void WindowedImageData(GArgReader& args)
{
	const char* szFilenameIn = args.pop_string();

	// Load the image
	GImage imageSource;
	loadPng(&imageSource, szFilenameIn);

	// Parse options
	bool reduced = false;
	int hstep = 1;
	int vstep = 1;
	int windowWidth = imageSource.width() / 2;
	int windowHeight = imageSource.height() / 2;
	int hole = 0;
	while(args.next_is_flag())
	{
		if(args.if_pop("-reduced"))
			reduced = true;
		else if(args.if_pop("-stepsizes"))
		{
			hstep = args.pop_uint();
			vstep = args.pop_uint();
		}
		else if(args.if_pop("-windowsize"))
		{
			windowWidth = args.pop_uint();
			windowHeight = args.pop_uint();
		}
		else if(args.if_pop("-hole"))
			hole = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the relation
	GImage imageWindow;
	imageWindow.setSize(windowWidth, windowHeight);
	GArffRelation rel;
	if(reduced)
	{
		rel.addAttribute("x", 0, NULL);
		rel.addAttribute("y", 0, NULL);
	}
	else
	{
		for(int y = 0; y < (int)imageWindow.height(); y++)
		{
			for(int x = 0; x < (int)imageWindow.width(); x++)
			{
				rel.addAttribute("r", 0, NULL);
				rel.addAttribute("g", 0, NULL);
				rel.addAttribute("b", 0, NULL);
			}
		}
	}

	// Generate the data
	int centerx = ((int)imageSource.width() - (int)imageWindow.width()) / 2;
	int centery = ((int)imageSource.height() - (int)imageWindow.height()) / 2;
	GMatrix data(0, reduced ? 2 : imageWindow.width() * imageWindow.height() * 3);
	for(int y = 0; y + (int)imageWindow.height() <= (int)imageSource.height(); y += vstep)
	{
		for(int x = 0; x + (int)imageWindow.width() <= (int)imageSource.width(); x += hstep)
		{
			if(hole > 0 && x > centerx - hole && x < centerx + hole && y > centery - hole && y < centery + hole)
				continue;
			if(reduced)
			{
				double* pVec = data.newRow();
				pVec[0] = x;
				pVec[1] = y;
			}
			else
			{
				GRect r(x, y, imageWindow.width(), imageWindow.height());
				imageWindow.blit(0, 0, &imageSource, &r);
				double* pVec = data.newRow();
				for(int yy = 0; yy < (int)imageWindow.height(); yy++)
				{
					for(int xx = 0; xx < (int)imageWindow.width(); xx++)
					{
						unsigned int pix = imageWindow.pixel(xx, yy);
						*(pVec++) = (double)gRed(pix) / 256;
						*(pVec++) = (double)gGreen(pix) / 256;
						*(pVec++) = (double)gBlue(pix) / 256;
					}
				}
			}
		}
	}

	// Print the data
	data.print(cout);
}

void addCraneToScene(GRayTraceScene& scene, double craneYaw, double ballHeight, double ballRadius, double x, double y, double z)
{
	double cranePitch = 1.0;
	double craneLength = 3.0;
	double craneRadius = 0.18;
	double cableRadius = 0.03;

	// Add the crane
	GRayTracePhysicalMaterial* pCraneMaterial = new GRayTracePhysicalMaterial();
	pCraneMaterial->setColor(GRayTraceMaterial::Diffuse, 0.9, 0.7, 0.3);
	pCraneMaterial->setColor(GRayTraceMaterial::Reflective, 0.45, 0.35, 0.15);
	scene.addMaterial(pCraneMaterial);
	G3DVector craneBottom, craneTop;
	//craneBottom.set(0, 0, 0);
	craneBottom.set(x - craneLength * cos(cranePitch) * sin(craneYaw), y - craneLength * sin(cranePitch), z - craneLength * cos(cranePitch) * (-cos(craneYaw)));
	craneTop.set(x + craneLength * cos(cranePitch) * sin(craneYaw), y + craneLength * sin(cranePitch), z + craneLength * cos(cranePitch) * (-cos(craneYaw)));
	GRayTraceTriMesh* pCrane = GRayTraceTriMesh::makeCylinder(pCraneMaterial, &craneBottom, &craneTop, craneRadius, 12, false/*end caps*/);
	pCrane->computePhongNormals();
	scene.addMesh(pCrane);

	// Add the cable
	GRayTracePhysicalMaterial* pCableMaterial = new GRayTracePhysicalMaterial();
	pCableMaterial->setColor(GRayTraceMaterial::Diffuse, 0.6, 0.6, 0.6);
	scene.addMaterial(pCableMaterial);
	G3DVector ballCenter;
	ballCenter.copy(&craneTop);
	ballCenter.m_vals[1] = y + ballHeight;
	GRayTraceTriMesh* pCable = GRayTraceTriMesh::makeCylinder(pCableMaterial, &ballCenter, &craneTop, cableRadius, 12, false/*end caps*/);
	pCable->computePhongNormals();
	scene.addMesh(pCable);

	// Add the ball
	GRayTracePhysicalMaterial* pBallMaterial = new GRayTracePhysicalMaterial();
	pBallMaterial->setColor(GRayTraceMaterial::Diffuse, 0.1, 0.1, 0.2);
	pBallMaterial->setColor(GRayTraceMaterial::Reflective, 0.2, 0.2, 0.4);
	scene.addMaterial(pBallMaterial);
	GRayTraceSphere* pBall = new GRayTraceSphere(pBallMaterial, ballCenter.m_vals[0], ballCenter.m_vals[1], ballCenter.m_vals[2], ballRadius);
	scene.addObject(pBall);
}

GImage* makeCraneImage(double craneYaw, double ballHeight, int wid, int hgt, double ballRadius, bool front)
{
	if(front)
		craneYaw = M_PI - craneYaw;

	// Make a scene
	GRand prng(0);
	GRayTraceScene scene(&prng);
	scene.setAmbientLight(0.3, 0.3, 0.3);
	scene.addLight(new GRayTraceDirectionalLight(-1.0, 2.0, 1.0, 0.1/*r*/, 0.2/*g*/, 0.3/*b*/, 0.0/*jitter*/));
	scene.addLight(new GRayTraceDirectionalLight(1.0, 0.0, 0.3, 0.10/*r*/, 0.15/*g*/, 0.05/*b*/, 0.0/*jitter*/));
	scene.addLight(new GRayTraceDirectionalLight(0.0, 0.0, 1.0, 0.15/*r*/, 0.05/*g*/, 0.10/*b*/, 0.0/*jitter*/));
	scene.setBackgroundColor(1/*a*/, 1.0/*r*/, 1.0/*g*/, 1.0/*b*/);
	GRayTraceCamera* pCamera = scene.camera();
	pCamera->setViewAngle(1.0);
	pCamera->setImageSize(wid, hgt);
	if(front)
		pCamera->lookFromPoint()->set(0.0, 0.7, 3.5);
	else
		pCamera->lookFromPoint()->set(0.0, 0.7, 1.0);
	G3DVector cameraDirection(0.0, 0.0, -1.0);
	pCamera->setDirection(&cameraDirection, 0.0);
	//scene.setRenderMode(GRayTraceScene::FAST_RAY_TRACE);
	scene.setRenderMode(GRayTraceScene::QUALITY_RAY_TRACE);

	addCraneToScene(scene, craneYaw, ballHeight, ballRadius, 0.0, 0.0, 0.0);

	// Render the image
	scene.render();
	return scene.releaseImage();
}

GImage* makeThreeCraneImage(double* craneYaw, double* ballHeight, int wid, int hgt, double ballRadius)
{
	craneYaw[0] = M_PI - craneYaw[0];
	craneYaw[1] = M_PI - craneYaw[1];
	craneYaw[2] = M_PI - craneYaw[2];

	// Make a scene
	GRand prng(0);
	GRayTraceScene scene(&prng);
	scene.setAmbientLight(0.3, 0.3, 0.3);
	scene.addLight(new GRayTraceDirectionalLight(-1.0, 2.0, 1.0, 0.1/*r*/, 0.2/*g*/, 0.3/*b*/, 0.0/*jitter*/));
	scene.addLight(new GRayTraceDirectionalLight(1.0, 0.0, 0.3, 0.10/*r*/, 0.15/*g*/, 0.05/*b*/, 0.0/*jitter*/));
	scene.addLight(new GRayTraceDirectionalLight(0.0, 0.0, 1.0, 0.15/*r*/, 0.05/*g*/, 0.10/*b*/, 0.0/*jitter*/));
	scene.setBackgroundColor(1/*a*/, 1.0/*r*/, 1.0/*g*/, 1.0/*b*/);
	GRayTraceCamera* pCamera = scene.camera();
	pCamera->setViewAngle(1.0);
	pCamera->setImageSize(wid, hgt);
	pCamera->lookFromPoint()->set(0.0, 0.7/*0.84*/, 4.2);
	G3DVector cameraDirection(0.0, 0.0, -1.0);
	pCamera->setDirection(&cameraDirection, 0.0);
	//scene.setRenderMode(GRayTraceScene::FAST_RAY_TRACE);
	scene.setRenderMode(GRayTraceScene::QUALITY_RAY_TRACE);

	addCraneToScene(scene, craneYaw[0], ballHeight[0], ballRadius, -1.0, 0.0, 0.0);
	addCraneToScene(scene, craneYaw[1], ballHeight[1], ballRadius, 0.0, 0.0, 0.0);
	addCraneToScene(scene, craneYaw[2], ballHeight[2], ballRadius, 1.0, 0.0, 0.0);

	// Render the image
	scene.render();
	return scene.releaseImage();
}

void CraneDataset(GArgReader& args)
{
	// Parse options
	int wid = 64;
	int hgt = 48;
	int horizFrames = 25;
	int vertFrames = 21;
	string imageFile = "";
	double ballRadius = 0.3;
	bool front = false;
	double blur = 0.0;
	bool gray = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-front"))
			front = true;
		else if(args.if_pop("-saveimage"))
			imageFile = args.pop_string();
		else if(args.if_pop("-ballradius"))
			ballRadius = args.pop_double();
		else if(args.if_pop("-frames"))
		{
			horizFrames = args.pop_uint();
			vertFrames = args.pop_uint();
		}
		else if(args.if_pop("-size"))
		{
			wid = args.pop_uint();
			hgt = args.pop_uint();
		}
		else if(args.if_pop("-blur"))
			blur = args.pop_double();
		else if(args.if_pop("-gray"))
			gray = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	GMatrix data(0, wid * hgt * (gray ? 1 : 3));
	GImage grid;
	grid.setSize(horizFrames * wid, vertFrames * hgt);
	double dx = (0.75 - (-0.75)) / (horizFrames - 1);
	double dy = (1.25 - 0.0) / (vertFrames - 1);

	for(int vert = 0; vert < vertFrames; vert++)
	{
		double ballHeight = (vertFrames - 1 - vert) * dy;
		for(int horiz = 0; horiz < horizFrames; horiz++)
		{
			double craneYaw = horiz * dx - 0.75;

			GImage* pImage = makeCraneImage(craneYaw, ballHeight, wid, hgt, ballRadius, front);
			if(blur > 0)
				pImage->blur(blur);
			Holder<GImage> hImage(pImage);

			GRect r(0, 0, wid, hgt);
			grid.blit(wid * horiz, hgt * vert, pImage, &r);
			grid.box(wid * horiz, hgt * vert, wid * horiz + wid - 1, hgt * vert + hgt - 1, 0xffb0b0b0);

			double* pRow = data.newRow();
			unsigned int* pPixels = pImage->pixels();
			for(int yy = 0; yy < hgt; yy++)
			{
				for(int xx = 0; xx < wid; xx++)
				{
					if(gray)
						*(pRow++) = gGray(*pPixels);
					else
					{
						*(pRow++) = gRed(*pPixels);
						*(pRow++) = gGreen(*pPixels);
						*(pRow++) = gBlue(*pPixels);
					}
					pPixels++;
				}
			}
		}
	}

	if(imageFile.length() > 0)
		savePng(&grid, imageFile.c_str());
	data.print(cout);
}

void cranePath(GArgReader& args)
{
	GMatrix* pActions = loadData(args.pop_string());
	Holder<GMatrix> hActions(pActions);

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	int wid = 64;
	int hgt = 48;
	int horizFrames = 25;
	int vertFrames = 21;
	double state[2];
	double ballRadius = 0.3;
	bool front = false;
	double blur = 0.0;
	bool gray = false;
	string stateFile = "";
	double transitionNoiseDev = 0.0;
	double observationNoiseDev = 0.0;
	while(args.next_is_flag())
	{
		if(args.if_pop("-front"))
			front = true;
		else if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-ballradius"))
			ballRadius = args.pop_double();
		else if(args.if_pop("-size"))
		{
			wid = args.pop_uint();
			hgt = args.pop_uint();
		}
		else if(args.if_pop("-blur"))
			blur = args.pop_double();
		else if(args.if_pop("-gray"))
			gray = true;
		else if(args.if_pop("-state"))
			stateFile = args.pop_string();
		else if(args.if_pop("-noise"))
		{
			transitionNoiseDev = args.pop_double();
			observationNoiseDev = args.pop_double();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the observation data
	GRand prng(nSeed);
	GMatrix data(0, wid * hgt * (gray ? 1 : 3));
	GMatrix stateData(0, 2);
	double dx = (0.75 - (-0.75)) / (horizFrames - 1);
	double dy = (1.25 - 0.0) / (vertFrames - 1);
	double posx = 0.0;
	double posy = 1.25 / 2.0;
	for(size_t i = 0; i < pActions->rows(); i++)
	{
		double ballHeight = 1.25 - posy;
		double craneYaw = posx;
		state[0] = (craneYaw + 0.75) * (horizFrames - 1) / 1.5;
		state[1] = ballHeight * (vertFrames - 1) / 1.25;
		GVec::copy(stateData.newRow(), state, 2);

		// Generate the image vector
		GImage* pImage = makeCraneImage(craneYaw, ballHeight, wid, hgt, ballRadius, front);
		if(blur > 0)
			pImage->blur(blur);
		Holder<GImage> hImage(pImage);
		double* pRow = data.newRow();
		unsigned int* pPixels = pImage->pixels();
		for(int yy = 0; yy < hgt; yy++)
		{
			for(int xx = 0; xx < wid; xx++)
			{
				if(gray)
					*(pRow++) = ClipChan((int)floor((double)gGray(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
				else
				{
					*(pRow++) = ClipChan((int)floor((double)gRed(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
					*(pRow++) = ClipChan((int)floor((double)gGreen(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
					*(pRow++) = ClipChan((int)floor((double)gBlue(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
				}
				pPixels++;
			}
		}

		// Move
		int a = (int)pActions->row(i)[0];
		double deltax = 0;
		double deltay = 0;
		switch(a)
		{
			case 0: deltax = -1.0; deltay = 0.0; break;
			case 1: deltax = 1.0; deltay = 0.0; break;
			case 2: deltax = 0.0; deltay = -1.0; break;
			case 3: deltax = 0.0; deltay = 1.0; break;
		}
		deltax += prng.normal() * transitionNoiseDev;
		deltay += prng.normal() * transitionNoiseDev;
		deltax *= dx;
		deltay *= dy;
		posx = std::min(0.75, std::max(-0.75, posx + deltax));
		posy = std::min(1.25, std::max(0.0, posy + deltay));
	}

	if(stateFile.length() > 0)
		stateData.saveArff(stateFile.c_str());
	data.print(cout);
}

void threeCranePath(GArgReader& args)
{
	GMatrix* pActions = loadData(args.pop_string());
	Holder<GMatrix> hActions(pActions);

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	int wid = 64;
	int hgt = 48;
	int horizFrames = 25;
	int vertFrames = 21;
	double state[4];
	double ballRadius = 0.3;
	double blur = 0.0;
	bool gray = false;
	string stateFile = "";
	double transitionNoiseDev = 0.0;
	double observationNoiseDev = 0.0;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-ballradius"))
			ballRadius = args.pop_double();
		else if(args.if_pop("-size"))
		{
			wid = args.pop_uint();
			hgt = args.pop_uint();
		}
		else if(args.if_pop("-blur"))
			blur = args.pop_double();
		else if(args.if_pop("-gray"))
			gray = true;
		else if(args.if_pop("-state"))
			stateFile = args.pop_string();
		else if(args.if_pop("-noise"))
		{
			transitionNoiseDev = args.pop_double();
			observationNoiseDev = args.pop_double();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the observation data
	GRand prng(nSeed);
	GMatrix data(0, wid * hgt * (gray ? 1 : 3));
	GMatrix stateData(0, 4);
	double dx = (0.75 - (-0.75)) / (horizFrames - 1);
	double dy = (1.25 - 0.0) / (vertFrames - 1);
	double posx[3]; posx[0] = 0.0; posx[1] = 0.0; posx[2] = 0.0;
	double posy[3]; posy[0] = 1.25 / 2.0; posy[1] = 1.25 / 2.0; posy[2] = 1.25 / 2.0;
	for(size_t i = 0; i < pActions->rows(); i++)
	{
		double ballHeight[3];
		ballHeight[0] = 1.25 - posy[0];
		ballHeight[1] = 1.25 - posy[1];
		ballHeight[2] = 1.25 - posy[2];
		double craneYaw[3];
		craneYaw[0] = posx[0];
		craneYaw[1] = posx[1];
		craneYaw[2] = posx[2];
		state[0] = (craneYaw[0] + 0.75) * (horizFrames - 1) / 1.5;
		state[1] = ballHeight[0] * (vertFrames - 1) / 1.25;
		state[2] = (craneYaw[2] + 0.75) * (horizFrames - 1) / 1.5;
		state[3] = ballHeight[2] * (vertFrames - 1) / 1.25;
		GVec::copy(stateData.newRow(), state, 4);

		// Generate the image vector
		GImage* pImage = makeThreeCraneImage(craneYaw, ballHeight, wid, hgt, ballRadius);
		if(blur > 0)
			pImage->blur(blur);
		Holder<GImage> hImage(pImage);
		double* pRow = data.newRow();
		unsigned int* pPixels = pImage->pixels();
		for(int yy = 0; yy < hgt; yy++)
		{
			for(int xx = 0; xx < wid; xx++)
			{
				if(gray)
					*(pRow++) = ClipChan((int)floor((double)gGray(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
				else
				{
					*(pRow++) = ClipChan((int)floor((double)gRed(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
					*(pRow++) = ClipChan((int)floor((double)gGreen(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
					*(pRow++) = ClipChan((int)floor((double)gBlue(*pPixels) + 0.5 + prng.normal() * 255.0 * observationNoiseDev));
				}
				pPixels++;
			}
		}

		// Move
		int a = (int)pActions->row(i)[0];
		double deltax = 0;
		double deltay = 0;
		switch(a)
		{
			case 0: deltax = -1.0; deltay = 0.0; break;
			case 1: deltax = 1.0; deltay = 0.0; break;
			case 2: deltax = 0.0; deltay = -1.0; break;
			case 3: deltax = 0.0; deltay = 1.0; break;
		}
		deltax += prng.normal() * transitionNoiseDev;
		deltay += prng.normal() * transitionNoiseDev;
		deltax *= dx;
		deltay *= dy;
		posx[0] = std::min(0.75, std::max(-0.75, posx[0] + deltax));
		posy[0] = std::min(1.25, std::max(0.0, posy[0] + deltay));

		a = (int)prng.next(4);
		switch(a)
		{
			case 0: deltax = -1.0; deltay = 0.0; break;
			case 1: deltax = 1.0; deltay = 0.0; break;
			case 2: deltax = 0.0; deltay = -1.0; break;
			case 3: deltax = 0.0; deltay = 1.0; break;
		}
		deltax += prng.normal() * transitionNoiseDev;
		deltay += prng.normal() * transitionNoiseDev;
		deltax *= dx;
		deltay *= dy;
		posx[1] = std::min(0.75, std::max(-0.75, posx[1] + deltax));
		posy[1] = std::min(1.25, std::max(0.0, posy[1] + deltay));

		a = (int)pActions->row(i)[1];
		switch(a)
		{
			case 0: deltax = -1.0; deltay = 0.0; break;
			case 1: deltax = 1.0; deltay = 0.0; break;
			case 2: deltax = 0.0; deltay = -1.0; break;
			case 3: deltax = 0.0; deltay = 1.0; break;
		}
		deltax += prng.normal() * transitionNoiseDev;
		deltay += prng.normal() * transitionNoiseDev;
		deltax *= dx;
		deltay *= dy;
		posx[2] = std::min(0.75, std::max(-0.75, posx[2] + deltax));
		posy[2] = std::min(1.25, std::max(0.0, posy[2] + deltay));
	}

	if(stateFile.length() > 0)
		stateData.saveArff(stateFile.c_str());
	data.print(cout);
}

void randomSequence(GArgReader& args)
{
	size_t len = args.pop_uint();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	size_t start = 0;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-start"))
			start = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the sequence
	GRand prng(nSeed);
	size_t* seq = new size_t[len];
	ArrayHolder<size_t> hSeq(seq);
	GIndexVec::makeIndexVec(seq, len);
	GIndexVec::shuffle(seq, len, &prng);

	// Print it
	for(size_t i = 0; i < len; i++)
		cout << (seq[i] + start) << "\n";
}

void ScaleAndRotate(GArgReader& args)
{
	// Load the image
	GImage imageSource;
	loadPng(&imageSource, args.pop_string());

	// Parse options
	int rotateFrames = 40;
	int scaleFrames = 15;
	double arc = 2.0 * M_PI;
	string imageFile = "";
	while(args.next_is_flag())
	{
		if(args.if_pop("-saveimage"))
			imageFile = args.pop_string();
		else if(args.if_pop("-frames"))
		{
			rotateFrames = args.pop_uint();
			scaleFrames = args.pop_uint();
		}
		else if(args.if_pop("-arc"))
			arc = args.pop_double();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the frames
	double finalSize = 0.4;
	GImage grid;
	grid.setSize(rotateFrames * imageSource.width(), scaleFrames * imageSource.height());
	GImage rotated;
	GImage frame;
	frame.setSize(imageSource.width(), imageSource.height());
	GMatrix data(0, imageSource.width() * imageSource.height());
	for(int y = 0; y < scaleFrames; y++)
	{
		for(int x = 0; x < rotateFrames; x++)
		{
			// Make the frame
			rotated.rotate(&imageSource, imageSource.width() / 2, imageSource.height() / 2, (double)x * arc / rotateFrames);
			GDoubleRect rSrc(1.0, 1.0, imageSource.width() - 2, imageSource.height() - 2);
			double s = (double)y / (scaleFrames - 1);
			GDoubleRect rDest(1.0, 1.0, (1.0 - s) * rSrc.w + s * finalSize * rSrc.w, (1.0 - s) * rSrc.h + s * finalSize * rSrc.h);
			rDest.x += (rSrc.w - rDest.w) / 2.0;
			rDest.y += (rSrc.h - rDest.h) / 2.0;
			frame.clear(0xffffffff);
			frame.blitStretchInterpolate(&rDest, &rotated, &rSrc);

			// Convert to a data row
			double* pRow = data.newRow();
			for(int b = 0; b < (int)imageSource.height(); b++)
			{
				for(int a = 0; a < (int)imageSource.width(); a++)
					*(pRow++) = gGray(frame.pixel(a, b));
			}

			// Save in the grid image
			GRect r(0, 0, imageSource.width(), imageSource.height());
			frame.box(0, 0, imageSource.width() - 1, imageSource.height() - 1, 0xffa0a0a0);
			grid.blit(x * imageSource.width(), y * imageSource.height(), &frame, &r);
		}
	}
	if(imageFile.length() > 0)
		savePng(&grid, imageFile.c_str());
	data.print(cout);
}

void gridRandomWalk(GArgReader& args)
{
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	int width = args.pop_uint();
	int height = (int)(pData->rows() / width);
	if((height * width) != (int)pData->rows())
		throw Ex("Expected a dataset with a number of rows that is divisible by width");
	size_t samples = args.pop_uint();
	if(pData->rows() < 2)
		throw Ex("Expected at least two states");

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	int x = width / 2;
	int y = height / 2;
	string obsFile = "observations.arff";
	string actionFile = "actions.arff";
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-start"))
		{
			x = args.pop_uint();
			y = args.pop_uint();
		}
		else if(args.if_pop("-obsfile"))
			obsFile = args.pop_string();
		else if(args.if_pop("-actionfile"))
			actionFile = args.pop_string();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Generate the dataset
	GRand prng(nSeed);
	GMatrix dataObs(pData->relation());
	sp_relation pControlRel = new GUniformRelation(1, 4);
	GMatrix dataControl(pControlRel);
	GMatrix dataState(0, 2);
	int colCount = (int)pData->cols();
	while(dataObs.rows() < samples)
	{
		double* pRow = dataState.newRow();
		pRow[0] = x;
		pRow[1] = y;
		GVec::copy(dataObs.newRow(), pData->row(width * y + x), colCount);
		int action = 0;
		while(true)
		{
			action = (int)prng.next(4);
			if(action == 0 && x > 0)
			{
				x--;
				break;
			}
			else if(action == 1 && x < width - 1)
			{
				x++;
				break;
			}
			else if(action == 2 && y > 0)
			{
				y--;
				break;
			}
			else if(action == 3 && y < height - 1)
			{
				y++;
				break;
			}
		}
		dataControl.newRow()[0] = action;
	}
	dataControl.saveArff(actionFile.c_str());
	dataObs.saveArff(obsFile.c_str());
	dataState.print(cout);
}

void vectorToImage(GImage* pImage, const double* pVec, int wid, int hgt)
{
	pImage->setSize(wid, hgt);
	unsigned int* pPix = pImage->pixels();
	for(int y = 0; y < hgt; y++)
	{
		for(int x = 0; x < wid; x++)
		{
			int r = ClipChan((int)*pVec++);
			int g = ClipChan((int)*pVec++);
			int b = ClipChan((int)*pVec++);
			*pPix = gARGB(0xff, r, g, b);
			pPix++;
		}
	}
}

void vectorToImage(GArgReader& args)
{
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	size_t r = args.pop_uint();
	if(r >= pData->rows())
		throw Ex("row index out of range");
	size_t wid = args.pop_uint();
	size_t channels = args.pop_uint();
	size_t hgt = pData->cols() / (wid * channels);
	double range = args.pop_double();
	if((wid * hgt * channels) != pData->cols())
		throw Ex("Invalid dimensions");
	double* pVec = pData->row(r);
	GImage image;
	GVec::toImage(pVec, &image, wid, hgt, channels, range);
	savePng(&image, "image.png");
}

void dataToFrames(GArgReader& args)
{
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	size_t wid = args.pop_uint();
	size_t channels = args.pop_uint();
	size_t hgt = pData->cols() / (wid * channels);
	double range = args.pop_double();
	if((wid * hgt * channels) != pData->cols())
		throw Ex("Invalid dimensions");
	GImage image;
	GImage master;
	master.setSize((unsigned int)wid, (unsigned int)(hgt * pData->rows()));
	for(unsigned int i = 0; i < pData->rows(); i++)
	{
		GVec::toImage(pData->row(i), &image, wid, hgt, channels, range);
		GRect r(0, 0, (int)wid, (int)hgt);
		master.blit(0, i * (int)hgt, &image, &r);
	}
	savePng(&master, "frames.png");
}

void sceneRobotSimulationGrid(GArgReader& args)
{
	const char* sceneFilename = args.pop_string();
	int strafeLen = args.pop_uint();
	int zoomLen = args.pop_uint();
	int cameraWid = args.pop_uint();
	int cameraHgt = args.pop_uint();
	GImage scene;
	loadPng(&scene, sceneFilename);
	if(scene.height() > scene.width() * cameraHgt / cameraWid)
		throw Ex("Expected a panoramic (wide) scene");
	double maxWid = scene.height() * cameraWid / cameraHgt;
	double maxStrafeStride = (scene.width() - maxWid) / strafeLen;

	GImage frame;
	frame.setSize(cameraWid, cameraHgt);

	GImage master;
	master.setSize(cameraWid * (strafeLen + 1), cameraHgt * (zoomLen + 1));
	for(double y = 0; y <= zoomLen; y++)
	{
		for(double x = 0; x <= strafeLen; x++)
		{
			if(y > 0.3 * zoomLen && x > 0.3 * strafeLen && x < 0.7 * strafeLen)
				continue;
			//double xx = x / strafeLen;
			double yy = y / zoomLen;
			double z = yy;
			double h = z * cameraHgt + (1.0 - z) * scene.height();
			double w = h * cameraWid / cameraHgt;
			double strafeStride = z + (1.0 - z) * maxStrafeStride;
			double left = ((double)scene.width() - w) / 2.0 + (x - strafeLen / 2.0) * strafeStride;
			double top = (scene.height() - h) / 2.0;
			GDoubleRect src(left, top, w, h);
			GDoubleRect dest(0, 0, cameraWid, cameraHgt);
			frame.blitStretchInterpolate(&dest, &scene, &src);
			GRect r(0, 0, cameraWid, cameraHgt);
			master.blit((int)(cameraWid * x), (int)(cameraHgt * y), &frame, &r);
		}
	}
	savePng(&master, "frames.png");
}

void sceneRobotSimulationPath(GArgReader& args)
{
	const char* sceneFilename = args.pop_string();
	unsigned int frames = args.pop_uint();

	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	double transitionNoise = 0.0;
	double observationNoise = 0.0;
	int cameraWid = 64;
	int cameraHgt = 48;
	int strafeLen = 30;
	int zoomLen = 20;
	string stateFilename = "state.arff";
	string actFilename = "act.arff";
	string obsFilename = "obs.arff";
	while(args.next_is_flag())
	{
		if(args.if_pop("-noise"))
		{
			transitionNoise = args.pop_double();
			observationNoise = args.pop_double();
		}
		else if(args.if_pop("-framesize"))
		{
			cameraWid = args.pop_uint();
			cameraHgt = args.pop_uint();
		}
		else if(args.if_pop("-gridsize"))
		{
			strafeLen = args.pop_uint();
			zoomLen = args.pop_uint();
		}
		else if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-out"))
		{
			stateFilename = args.pop_string();
			actFilename = args.pop_string();
			obsFilename = args.pop_string();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	GImage scene;
	loadPng(&scene, sceneFilename);
	if(scene.height() > scene.width() * cameraHgt / cameraWid)
		throw Ex("Expected a panoramic (wide) scene");
	double maxWid = scene.height() * cameraWid / cameraHgt;
	double maxStrafeStride = (scene.width() - maxWid) / strafeLen;
	GRand prng(seed);

	GImage frame;
	frame.setSize(cameraWid, cameraHgt);
	double x = strafeLen / 2.0;
	double y = 0.0;//zoomLen / 2.0;
	sp_relation pRel = new GUniformRelation(1, 4);
	GMatrix state(0, 2);
	GMatrix act(pRel);
	GMatrix obs(0, cameraWid * cameraHgt * 3);
	for(unsigned int i = 0; i < frames; i++)
	{
		// Make the state
		double* pVec = state.newRow();
		pVec[0] = x;
		pVec[1] = y;

		// Make the frame
		double yy = y / zoomLen;
		double z = yy;
		double h = z * cameraHgt + (1.0 - z) * scene.height();
		double w = h * cameraWid / cameraHgt;
		double strafeStride = z + (1.0 - z) * maxStrafeStride;
		double left = ((double)scene.width() - w) / 2.0 + (x - strafeLen / 2.0) * strafeStride;
		double top = (scene.height() - h) / 2.0;
		GDoubleRect src(left, top, w, h);
		GDoubleRect dest(0, 0, cameraWid, cameraHgt);
		frame.blitStretchInterpolate(&dest, &scene, &src);

		// Convert to an observation vector
		pVec = obs.newRow();
		unsigned int* pix = frame.pixels();
		for(unsigned int yy = 0; yy < frame.height(); yy++)
		{
			for(unsigned int xx = 0; xx < frame.width(); xx++)
			{
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gRed(*pix));
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gGreen(*pix));
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gBlue(*pix));
				pix++;
			}
		}

		// Do a random action
		double oldx = x;
		double oldy = y;
		int a;
		while(true)
		{
			a = (int)prng.next(4);
			if(a == 0)
				x -= 1.0;
			else if(a == 1)
				x += 1.0;
			else if(a == 2)
				y -= 1.0;
			else if(a == 3)
				y += 1.0;
			x += transitionNoise * prng.normal();
			y += transitionNoise * prng.normal();
			if(x < 0 || y < 0 || x > strafeLen || y > zoomLen)
			{
				x = oldx;
				y = oldy;
				continue;
			}
			if(y > 0.4 * zoomLen && y < 0.6 * zoomLen && x > 0.4 * strafeLen && x < 0.6 * strafeLen)
			{
				x = oldx;
				y = oldy;
				continue;
			}
			break;
		}
		act.newRow()[0] = a;
	}
	state.saveArff(stateFilename.c_str());
	obs.saveArff(obsFilename.c_str());
	act.saveArff(actFilename.c_str());
}

void mechanicalRabbit(GArgReader& args)
{
	const char* sceneFilename = args.pop_string();
	GDom doc;
	doc.loadJson(args.pop_string());
	GMatrix* pActions = loadData(args.pop_string());
	Holder<GMatrix> hActions(pActions);

	// Parse the options
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	string contextFilename = "context.arff";
	string stateFilename = "state.arff";
	string plannedObsFilename = "planned_obs.arff";
	string actualObsFilename = "actual_obs.arff";
	string actionsFilename = "actions.arff";
	double transitionNoise = 0.0;
	double observationNoise = 0.0;
	int cameraWid = 64;
	int cameraHgt = 48;
	int strafeLen = 30;
	int zoomLen = 20;
	while(args.next_is_flag())
	{
		if(args.if_pop("-out"))
		{
			contextFilename = args.pop_string();
			stateFilename = args.pop_string();
			plannedObsFilename = args.pop_string();
			actualObsFilename = args.pop_string();
			actionsFilename = args.pop_string();
		}
		else if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-noise"))
		{
			transitionNoise = args.pop_double();
			observationNoise = args.pop_double();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Generate the context sequence
	GRand prng(seed);
	GRecurrentModel rm(doc.root(), &prng);
	GMatrix obsPlanned(0, rm.obsDims());
	GMatrix context(0, rm.contextDims());
	GVec::setAll(rm.context(), 0.0, rm.contextDims());
	GVec::copy(context.newRow(), rm.context(), rm.contextDims());
	rm.predict(obsPlanned.newRow());
	for(size_t i = 0; i < pActions->rows(); i++)
	{
		rm.doAction(pActions->row(i));
		GVec::copy(context.newRow(), rm.context(), rm.contextDims());
		rm.predict(obsPlanned.newRow());
	}

	// Reset the model and the system
	GVec::setAll(rm.context(), 0.0, rm.contextDims());
	GImage scene;
	loadPng(&scene, sceneFilename);
	if(scene.height() > scene.width() * cameraHgt / cameraWid)
		throw Ex("Expected a panoramic (wide) scene");
	double maxWid = scene.height() * cameraWid / cameraHgt;
	double maxStrafeStride = (scene.width() - maxWid) / strafeLen;
	GImage frame;
	frame.setSize(cameraWid, cameraHgt);
	double x = strafeLen / 2.0;
	double y = 0.0;
	GMatrix obsActual(0, rm.obsDims());
	GMatrix state(0, 2);
	sp_relation pRel = new GUniformRelation(1, 4);
	GMatrix actionsActual(pRel);

	// Do the mechanical rabbit
	GTEMPBUF(double, tmpContext, rm.contextDims());
	size_t r = 0;
	size_t gap = 1;
	int safety = 0;
	while(r < context.rows() && gap > 0)
	{
		// Store the actual state
		double* pState = state.newRow();
		pState[0] = x;
		pState[1] = y;

		// Make the frame
		double yy = y / zoomLen;
		double z = yy;
		double h = z * cameraHgt + (1.0 - z) * scene.height();
		double w = h * cameraWid / cameraHgt;
		double strafeStride = z + (1.0 - z) * maxStrafeStride;
		double left = ((double)scene.width() - w) / 2.0 + (x - strafeLen / 2.0) * strafeStride;
		double top = (scene.height() - h) / 2.0;
		GDoubleRect src(left, top, w, h);
		GDoubleRect dest(0, 0, cameraWid, cameraHgt);
		frame.blitStretchInterpolate(&dest, &scene, &src);

		// Convert to an observation vector
		double* pVec = obsActual.newRow();
		double* pObsActual = pVec;
		unsigned int* pix = frame.pixels();
		for(unsigned int yy = 0; yy < frame.height(); yy++)
		{
			for(unsigned int xx = 0; xx < frame.width(); xx++)
			{
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gRed(*pix));
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gGreen(*pix));
				*(pVec++) = ClipChan((int)(observationNoise * 255 * prng.normal()) + gBlue(*pix));
				pix++;
			}
		}

		// Advance the mechanical rabbit
		rm.calibrate(pObsActual);
		if(r + 1 == context.rows())
			gap--;
		safety++;
		while(safety > 3 || (r + 1 < context.rows() && sqrt(GVec::squaredDistance(rm.context(), context.row(r), rm.contextDims())) < gap))
		{
			r++;
			safety = 0;
		}

		// Pick the best action
		int bestAction = -1;
		double bestSquaredDist = 1e308;
		double dAction;
		for(int i = 0; i < 4; i++)
		{
			GVec::copy(tmpContext, rm.context(), rm.contextDims());
			dAction = (double)i;
			rm.doAction(&dAction);
			double squaredDist = GVec::squaredDistance(context.row(r), rm.context(), rm.contextDims());
			if(bestAction < 0 || squaredDist < bestSquaredDist)
			{
				bestAction = i;
				bestSquaredDist = squaredDist;
			}
			GVec::copy(rm.context(), tmpContext, rm.contextDims());
		}

		// Apply the action to the model
		dAction = (double)bestAction;
		rm.doAction(&dAction);

		// Apply the action to the system
		actionsActual.newRow()[0] = bestAction;
		if(bestAction == 0)
			x -= 1.0;
		else if(bestAction == 1)
			x += 1.0;
		else if(bestAction == 2)
			y -= 1.0;
		else if(bestAction == 3)
			y += 1.0;
		x += transitionNoise * prng.normal();
		y += transitionNoise * prng.normal();
	}

	// Save the results
	context.saveArff(contextFilename.c_str());
	obsPlanned.saveArff(plannedObsFilename.c_str());
	state.saveArff(stateFilename.c_str());
	obsActual.saveArff(actualObsFilename.c_str());
	actionsActual.saveArff(actionsFilename.c_str());
}

void manifold(GArgReader& args)
{
	// Get the params
	size_t samples = args.pop_uint();

	// Parse the params
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.pop_string());
	}

	// Parse the expression
	string expr;
	while(args.size() > 0)
		expr += args.pop_string();
	GFunctionParser mfp(expr.c_str());

	// Parse the equations
	vector<GFunction*> funcs;
	size_t intrinsicDims = 0;
	char funcName[32];
	size_t equations = 0;
	while(true)
	{
		sprintf(funcName, "y%d", (int)equations + 1);
		GFunction* pFunc = mfp.getFunctionNoThrow(funcName);
		if(!pFunc)
		{
			if(equations == 0)
				throw Ex("There is no function named \"y1\". You must name your functions y1, y2, ...");
			break;
		}
		if(equations == 0)
			intrinsicDims = pFunc->m_expectedParams;
		else
		{
			if((size_t)pFunc->m_expectedParams != intrinsicDims)
				throw Ex("Function ", funcName, " has a different number of parameters than function y1. All of the y# functions must have the same number of parameters.");
		}
		funcs.push_back(pFunc);
		equations++;
	}

	// Generate the data
	GRand prng(seed);
	vector<double> params;
	params.resize(intrinsicDims);
	GMatrix data(0, funcs.size() + 1);
	data.newRows(samples);
	for(size_t i = 0; i < samples; i++)
	{
		for(size_t j = 0; j < intrinsicDims; j++)
			params[j] = prng.uniform();
		double* pRow = data[i];
		for(size_t j = 0; j < funcs.size(); j++)
			pRow[j] = funcs[j]->call(params);
		pRow[funcs.size()] = params[0];
	}

	// Sort by the first intrinsic parameter, then drop that column from the data
	data.sort(funcs.size());
	data.deleteColumn(funcs.size());

	// Print results
	data.print(cout);
}

void MakeAttributeSummaryGraph(GRelation* pRelation, GMatrix* pData, GImage* pImage, int attr)
{
	if(pRelation->valueCount(attr) == 0)
	{
		pImage->clear(0xffffffff);
		GHistogram hist(*pData, attr, UNKNOWN_REAL_VALUE, UNKNOWN_REAL_VALUE, (size_t)pImage->width());
		double height = hist.binLikelihood(hist.modeBin());
		GPlotWindow pw(pImage, hist.xmin(), 0.0, hist.xmax(), height);
		for(int i = 0; i < (int)pImage->width(); i++)
		{
			double x, y;
			pw.viewToWindow(i, 0, &x, &y);
			size_t bin = hist.xToBin(x);
			double likelihood = hist.binLikelihood(bin);
			if(likelihood > 0.0)
				pw.line(x, 0.0, x, likelihood, 0xff000080);
		}
	}
	else
	{
		size_t buckets = pRelation->valueCount(attr);
		GTEMPBUF(double, hist, buckets);
		GVec::setAll(hist, 0.0, buckets);
		for(size_t i = 0; i < pData->rows(); i++)
		{
			int b = (int)pData->row(i)[attr];
			if(b >= 0 && (size_t)b < buckets)
				hist[b]++;
		}

		// Plot it
		pImage->clear(0xffffffff);
		size_t max = 0;
		for(size_t i = 1; i < buckets; i++)
		{
			if(hist[i] > hist[max])
				max = i;
		}
		for(int i = 0; i < (int)pImage->width(); i++)
		{
			size_t b = i * buckets / pImage->width();
			int h = (int)(hist[b] * pImage->height() / hist[max]);
			pImage->line(i, pImage->height(), i, pImage->height() - h, (((b & 1) == 0) ? 0xff400000 : 0xff008040));
		}
	}
}

void MakeCorrelationGraph(GRelation* pRelation, GMatrix* pData, GImage* pImage, int attrx, int attry, double jitter, GRand* pRand)
{
	pImage->clear(0xffffffff);
	double xmin, ymin, xmax, ymax;
	bool bothNominal = true;
	if(pRelation->valueCount(attrx) == 0) //Continuous x attribute
	{
		xmin = pData->columnMin(attrx);
		xmax = pData->columnMax(attrx);
		bothNominal = false;
	}
	else //Discrete x attribute
	{
		xmin = -0.5;
		xmax = pRelation->valueCount(attrx) - 0.5;
	}
	if(pRelation->valueCount(attry) == 0) //Continuous y attribute
	{
		ymin = pData->columnMin(attry);
		ymax = pData->columnMax(attry);
		bothNominal = false;
	}
	else //Discrete y atrribute
	{
		ymin = -0.5;
		ymax = pRelation->valueCount(attry) - 0.5;
	}
	if(bothNominal)
	{
		GPlotWindow pw(pImage, 0.0, 0.0, 1.0, 1.0);
		double left = 0.0;
		double right = 0.0;
		size_t tot = pData->rows();
		for(size_t i = 0; i < pRelation->valueCount(attrx); i++)
		{
			GMatrix tmp(pData->relation());
			pData->splitByNominalValue(&tmp, attrx, (int)i);
			right += (double)tmp.rows() / tot;
			double bot = 0.0;
			double top = 0.0;
			for(size_t j = 0; j < pRelation->valueCount(attry); j++)
			{
				top += (double)tmp.countValue(attry, (double)j) / tmp.rows();
				int l, b, r, t;
				pw.windowToView(left, bot, &l, &b);
				pw.windowToView(right, top, &r, &t);
				pImage->boxFill(l, t, r - l, b - t, gAHSV(0xff, 0.9f * j / pRelation->valueCount(attry), 0.6f + ((i & 1) ? 0.0f : 0.4f), 0.4f + ((i & 1) ? 0.4f : 0.0f)));
				bot = top;
			}
			pData->mergeVert(&tmp);
			left = right;
		}
	}
	else
	{
		GPlotWindow pw(pImage, xmin, ymin, xmax, ymax);
		size_t samples = 2048;
		for(size_t i = 0; i < samples; i++)
		{
			double* pPat = pData->row(i * pData->rows() / samples);
			pw.point(pPat[attrx] + pRand->normal() * jitter * (xmax - xmin), pPat[attry] + pRand->normal() * jitter * (ymax - ymin), 0xff000080);
		}
	}
}

void MakeCorrelationLabel(GArffRelation* pRelation, GMatrix* pData, GImage* pImage, int attr, unsigned int bgCol)
{
	pImage->clear(bgCol);
	if(pRelation->valueCount(attr) == 0)
	{
		pImage->text(pRelation->attrName(attr), 0, 0, 1.0f, 0xff400000);
		double min = pData->columnMin(attr);
		double max = pData->columnMax(attr);

		for(int i = 0; i < 2; i++)
		{
			GImage image2;
			image2.setSize(pImage->width() - 16, 16);
			image2.clear(0);
			int xx = 0;
			char szValue[64];
			sprintf(szValue, "%.4lg", (i == 0 ? min : max));
			int eatspace = pImage->width() - 16 - GImage::measureTextWidth(szValue, 1.0f);
			xx += eatspace;
			image2.text(szValue, xx, 0, 1.0f, 0xff400000);
			GImage image3;
			image3.rotateClockwise90(&image2);
			GRect r3(0, 0, image3.width(), image3.height());
			pImage->blitAlpha((i == 0 ? 0 : pImage->width() - 16), 16, &image3, &r3);
		}
	}
	else
	{
		pImage->text(pRelation->attrName(attr), 0, 0, 1.0f, 0xff000040);
		GImage image2;
		image2.setSize(pImage->width() - 16, 16);
		image2.clear(0);
		GRect r2(0, 0, pImage->width() - 16, 16);

		int valueCount = (int)pRelation->valueCount(attr);
		for(int i = 0; i < valueCount; i++)
		{
			GImage image2;
			image2.setSize(pImage->width() - 16, 16);
			image2.clear(0);
			int xx = 0;
			ostringstream oss;
			pRelation->printAttrValue(oss, attr, i);
			string sValue = oss.str();
			int eatspace = pImage->width() - 16 - GImage::measureTextWidth(sValue.c_str(), 1.0f);
			xx += eatspace;
			image2.text(sValue.c_str(), xx, 0, 1.0f, 0xff000040);
			GImage image3;
			image3.rotateClockwise90(&image2);
			GRect r3(0, 0, image3.width(), image3.height());
			int span = pImage->width() / valueCount;
			int start = std::max(0, (span - 16) / 2);
			pImage->blitAlpha(start + span * i, 16, &image3, &r3);
		}
	}
}

void PlotCorrelations(GArgReader& args)
{
	// Load the data
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	GArffRelation* pRel = (GArffRelation*)pData->relation().get();

	// Parse options
	string filename = "plot.png";
	int cellsize = 120;
	int bordersize = 4;
	unsigned int bgCol = 0xffd0d0e0;
	size_t maxAttrs = 30;
	double jitter = 0.03;
	while(args.next_is_flag())
	{
		if(args.if_pop("-out"))
			filename = args.pop_string();
		else if(args.if_pop("-cellsize"))
			cellsize = args.pop_uint();
		else if(args.if_pop("-jitter"))
			jitter = args.pop_double();
		else if(args.if_pop("-maxattrs"))
			maxAttrs = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Make the chart
	GImage imageBig;
	int wid = (int)(std::min(maxAttrs, pRel->size()) + 1) * (cellsize + bordersize);
	imageBig.setSize(wid, wid);
	imageBig.clear(bgCol);
	GRand prng(getpid() * (unsigned int)time(NULL));
	GImage imageCell;
	GImage imageCell2;
	imageCell.setSize(cellsize, cellsize);
	for(size_t i = 0; i < pRel->size() && i < maxAttrs; i++)
	{
		MakeCorrelationLabel(pRel, pData, &imageCell, (int)i, bgCol);
		GRect r(0, 0, cellsize, cellsize);
		imageBig.blit(((int)i + 1) * (cellsize + bordersize), 0, &imageCell, &r);
		imageCell2.rotateCounterClockwise90(&imageCell);
		imageBig.blit(0, ((int)i + 1) * (cellsize + bordersize), &imageCell2, &r);
	}
	for(size_t y = 0; y < pRel->size() && y < maxAttrs; y++)
	{
		for(size_t x = 0; x < pRel->size() && x < maxAttrs; x++)
		{
			if(x == y)
				MakeAttributeSummaryGraph(pRel, pData, &imageCell, (int)x);
			else
				MakeCorrelationGraph(pRel, pData, &imageCell, (int)x, (int)y, jitter, &prng);
			GRect r(0, 0, cellsize, cellsize);
			imageBig.blit(((int)x + 1) * (cellsize + bordersize), ((int)y + 1) * (cellsize + bordersize), &imageCell, &r);
		}
	}
	savePng(&imageBig, filename.c_str());
	cout << "Output saved to " << filename.c_str() << ".\n";
}


void ppmToPng(GArgReader& args)
{
	GImage image;
	image.loadPpm(args.pop_string());
	savePng(&image, args.pop_string());
}

class Compare3DPointsByDistanceFromCameraFunctor
{
protected:
	GCamera* m_pCamera;

public:
	Compare3DPointsByDistanceFromCameraFunctor(GCamera* pCamera)
	: m_pCamera(pCamera)
	{
	}

	// returns false if pA is closer than pB
	bool operator() (const double* pA, const double* pB) const
	{
		G3DVector a, b, c, d;
		a.m_vals[0] = pA[0];
		a.m_vals[1] = pA[1];
		a.m_vals[2] = pA[2];
		b.m_vals[0] = pB[0];
		b.m_vals[1] = pB[1];
		b.m_vals[2] = pB[2];
		m_pCamera->project(&a, &c);
		m_pCamera->project(&b, &d);
		return (c.m_vals[2] > d.m_vals[2]);
	}
};

void toImageCoords(GImage* pImage, GCamera* pCamera, G3DVector* pIn, G3DVector* pOut)
{
	pCamera->project(pIn, pOut);

	// Flip the Y value, because positive is down in image coordinates
	pOut->m_vals[1] = pImage->height() - 1 - pOut->m_vals[1];
}

void Plot3d(GImage* pImage, GMatrix* pData, unsigned int bgCol, float pointRadius, double cameraDist, G3DVector* pCameraDirection, bool box, bool labels)
{
	GCamera camera(pImage->width(), pImage->height());
	camera.setViewAngle(M_PI / 3);
	G3DVector mean;
	mean.m_vals[0] = pData->columnMean(0);
	mean.m_vals[1] = pData->columnMean(1);
	mean.m_vals[2] = pData->columnMean(2);
	G3DVector min, max, range;
	min.m_vals[0] = pData->columnMin(0);
	range.m_vals[0] = pData->columnMax(0) - min.m_vals[0];
	min.m_vals[1] = pData->columnMin(1);
	range.m_vals[1] = pData->columnMax(1) - min.m_vals[1];
	min.m_vals[2] = pData->columnMin(2);
	range.m_vals[2] = pData->columnMax(2) - min.m_vals[2];
	max.copy(&range);
	max.add(&min);
	G3DReal dist = sqrt(min.squaredDist(&max)) * cameraDist;
	G3DVector* pCameraPos = camera.lookFromPoint();
	pCameraPos->copy(pCameraDirection);
	pCameraPos->multiply(-1);
	pCameraPos->normalize();
	pCameraPos->multiply(dist);
	pCameraPos->add(&mean);
	camera.setDirection(pCameraDirection, 0.0);

	G3DVector point, coords, point2, coords2;
	pImage->clear(bgCol);

	// Draw box
	if(box)
	{
		min.subtract(&mean);
		min.multiply(1.1);
		min.add(&mean);
		max.subtract(&mean);
		max.multiply(1.1);
		max.add(&mean);
		range.multiply(1.1);
		int x, y, z;
		for(z = 0; z < 2; z++)
		{
			for(y = 0; y < 2; y++)
			{
				for(x = 0; x < 2; x++)
				{
					unsigned int col = 0xff808080;
					if(x == 0 && y == 0 && z == 0)
						col = 0xff8080ff;
					if(x == 0)
					{
						point.set(min.m_vals[0], min.m_vals[1] + y * range.m_vals[1], min.m_vals[2] + z * range.m_vals[2]);
						point2.set(max.m_vals[0], min.m_vals[1] + y * range.m_vals[1], min.m_vals[2] + z * range.m_vals[2]);
						toImageCoords(pImage, &camera, &point, &coords);
						toImageCoords(pImage, &camera, &point2, &coords2);
						pImage->line((int)coords.m_vals[0], (int)coords.m_vals[1], (int)coords2.m_vals[0], (int)coords2.m_vals[1], col);
					}
					if(y == 0)
					{
						point.set(min.m_vals[0] + x * range.m_vals[0], min.m_vals[1], min.m_vals[2] + z * range.m_vals[2]);
						point2.set(min.m_vals[0] + x * range.m_vals[0], max.m_vals[1], min.m_vals[2] + z * range.m_vals[2]);
						toImageCoords(pImage, &camera, &point, &coords);
						toImageCoords(pImage, &camera, &point2, &coords2);
						pImage->line((int)coords.m_vals[0], (int)coords.m_vals[1], (int)coords2.m_vals[0], (int)coords2.m_vals[1], col);
					}
					if(z == 0)
					{
						point.set(min.m_vals[0] + x * range.m_vals[0], min.m_vals[1] + y * range.m_vals[1], min.m_vals[2]);
						point2.set(min.m_vals[0] + x * range.m_vals[0], min.m_vals[1] + y * range.m_vals[1], max.m_vals[2]);
						toImageCoords(pImage, &camera, &point, &coords);
						toImageCoords(pImage, &camera, &point2, &coords2);
						pImage->line((int)coords.m_vals[0], (int)coords.m_vals[1], (int)coords2.m_vals[0], (int)coords2.m_vals[1], col);
					}
				}
			}
		}

		// Draw axis labels
		if(labels)
		{
			{
				char tmp[32];
				GPlotLabelSpacer pls(min.m_vals[0], max.m_vals[0], 10);
				for(int i = 0; i < pls.count(); i++)
				{
					point.set(pls.label(i), min.m_vals[1], min.m_vals[2]);
					toImageCoords(pImage, &camera, &point, &coords);
					pImage->dot((float)coords.m_vals[0], (float)coords.m_vals[1], 3.0, 0xff404040, bgCol);
					sprintf(tmp, "%.5lg", pls.label(i));
					pImage->text(tmp, (int)coords.m_vals[0] + 4, (int)coords.m_vals[1] - 4, 1.0f, 0xff404040);
				}
			}
			{
				char tmp[32];
				GPlotLabelSpacer pls(min.m_vals[1], max.m_vals[1], 10);
				for(int i = 0; i < pls.count(); i++)
				{
					point.set(min.m_vals[0], pls.label(i), min.m_vals[2]);
					toImageCoords(pImage, &camera, &point, &coords);
					pImage->dot((float)coords.m_vals[0], (float)coords.m_vals[1], 3.0, 0xff404040, bgCol);
					sprintf(tmp, "%.5lg", pls.label(i));
					pImage->text(tmp, (int)coords.m_vals[0] + 4, (int)coords.m_vals[1] - 4, 1.0f, 0xff404040);
				}
			}
			{
				char tmp[32];
				GPlotLabelSpacer pls(min.m_vals[2], max.m_vals[2], 10);
				for(int i = 0; i < pls.count(); i++)
				{
					point.set(min.m_vals[0], min.m_vals[1], pls.label(i));
					toImageCoords(pImage, &camera, &point, &coords);
					pImage->dot((float)coords.m_vals[0], (float)coords.m_vals[1], 3.0, 0xff404040, bgCol);
					sprintf(tmp, "%.5lg", pls.label(i));
					pImage->text(tmp, (int)coords.m_vals[0] + 4, (int)coords.m_vals[1] - 4, 1.0f, 0xff404040);
				}
			}
		}
	}

	// Plot the points
	Compare3DPointsByDistanceFromCameraFunctor comparator(&camera);
	GMatrix copy(pData->rows(), 4);
	copy.copyColumns(0, pData, 0, 3);
	for(size_t i = 0; i < copy.rows(); i++)
		copy.row(i)[3] = (double)i;
	copy.sort(comparator);
	for(size_t i = 0; i < copy.rows(); i++)
	{
		double* pVec = copy.row(i);
		point.set(pVec[0], pVec[1], pVec[2]);
		toImageCoords(pImage, &camera, &point, &coords);
		float radius = pointRadius / (float)coords.m_vals[2];
		pImage->dot((float)coords.m_vals[0], (float)coords.m_vals[1], radius, gAHSV(0xff, 0.8f * (float)pVec[3] / copy.rows(), 1.0f, 0.5f), bgCol);
	}
}

void Plot3dMulti(GArgReader& args)
{
	// Load
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	GArffRelation* pRel = (GArffRelation*)pData->relation().get();

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	int horizPlots = 1;
	int vertPlots = 1;
	int wid = 1000;
	int hgt = 1000;
	string filename = "plot.png";
	float pointRadius = 40.0f;
	double cameraDistance = 1.5;
	bool box = true;
	bool labels = true;
	unsigned int cBackground = 0xffffffff;
	G3DVector cameraDirection;
	cameraDirection.set(0.6, -0.3, -0.8);
	bool blast = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-blast"))
			blast = true;
		else if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-out"))
			filename = args.pop_string();
		else if(args.if_pop("-size"))
		{
			wid = args.pop_uint();
			hgt = args.pop_uint();
		}
		else if(args.if_pop("-pointradius"))
			pointRadius = (float)args.pop_double();
		else if(args.if_pop("-bgcolor"))
			cBackground = hexToRgb(args.pop_string());
		else if(args.if_pop("-cameradistance"))
			cameraDistance = args.pop_double();
		else if(args.if_pop("-cameradirection"))
		{
			cameraDirection.m_vals[0] = args.pop_double();
			cameraDirection.m_vals[1] = args.pop_double();
			cameraDirection.m_vals[2] = args.pop_double();
		}
		else if(args.if_pop("-nobox"))
			box = false;
		else if(args.if_pop("-nolabels"))
			labels = false;
		else
			throw Ex("Invalid option: ", args.peek());
	}
	if(blast)
	{
		pointRadius /= 5;
		wid /= 5;
		hgt /= 5;
		horizPlots *= 5;
		vertPlots *= 5;
	}

	// Check values
	if(pRel->size() != 3)
		throw Ex("Sorry, only data with 3 dims is currently supported");
	if(!pRel->areContinuous(0,3))
		throw Ex("Sorry, only continuous attributes are currently supported");

	// Make plots
	GRand prng(nSeed);
	GImage masterImage;
	masterImage.setSize(horizPlots * wid, vertPlots * hgt);
	GImage tmpImage;
	tmpImage.setSize(wid, hgt);
	for(int y = 0; y < vertPlots; y++)
	{
		for(int x = 0; x < horizPlots; x++)
		{
			if(blast)
			{
				cameraDirection.m_vals[0] = prng.normal();
				cameraDirection.m_vals[1] = prng.normal();
				cameraDirection.m_vals[2] = prng.normal();
				cameraDirection.normalize();
				cout << "row " << y << ", col " << x << ", cameradirection " << cameraDirection.m_vals[0] << " " << cameraDirection.m_vals[1] << " " << cameraDirection.m_vals[2] << "\n";
			}
			Plot3d(&tmpImage, pData, cBackground, pointRadius, cameraDistance, &cameraDirection, box, labels);
			GRect r(0, 0, wid, hgt);
			masterImage.blit(wid * x, hgt * y, &tmpImage, &r);
		}
	}
	savePng(&masterImage, filename.c_str());
	cout << "Plot saved to " << filename.c_str() << ".\n";
}

void model(GArgReader& args)
{
	// Load the model
	GDom doc;
	if(args.size() < 1)
		throw Ex("Model not specified");
	doc.loadJson(args.pop_string());
	GRand prng(0);
	GLearnerLoader ll(prng, true);
	GSupervisedLearner* pModeler = ll.loadSupervisedLearner(doc.root());
	Holder<GSupervisedLearner> hModeler(pModeler);

	// Load the data
	if(args.size() < 1)
		throw Ex("Expected the filename of a dataset");
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	if(pData->cols() != pModeler->relFeatures()->size() + pModeler->relLabels()->size())
		throw Ex("Model was trained with a different number of attributes than in this data");

	// Get other parameters
	unsigned int attrx = args.pop_uint();
	if(pData->relation()->valueCount(attrx) != 0)
		throw Ex("Sorry, currently only continuous attributes can be plotted");
	unsigned int attry = args.pop_uint();
	if(pData->relation()->valueCount(attry) != 0)
		throw Ex("Sorry, currently only continuous attributes can be plotted");
	size_t featureDims = pModeler->relFeatures()->size();
	if(attrx >= (unsigned int)featureDims || attry >= (unsigned int)featureDims)
		throw Ex("feature attribute out of range");

	// Parse options
	int width = 400;
	int height = 400;
	int labelDim = 0;
	float dotRadius = 3.0f;
	string filename = "plot.png";
	while(args.next_is_flag())
	{
		if(args.if_pop("-size"))
		{
			width = args.pop_uint();
			height = args.pop_uint();
		}
		else if(args.if_pop("-pointradius"))
			dotRadius = (float)args.pop_double();
		else if(args.if_pop("-out"))
			filename = args.pop_string();
		else
			throw Ex("Invalid option: ", args.peek());
	}


	// Compute label range
	double labelMin = 0.0;
	double labelRange = (double)pData->relation()->valueCount(featureDims + labelDim);
	if(labelRange == 0.0)
	{
		labelMin = pData->columnMin(featureDims + labelDim);
		labelRange = pData->columnMax(featureDims + labelDim) - labelMin;
	}

	// Plot the data
	double xmin, xrange, ymin, yrange;
	xmin = pData->columnMin(attrx);
	xrange = pData->columnMax(attrx) - xmin;
	ymin = pData->columnMin(attry);
	yrange = pData->columnMax(attry) - ymin;
	GImage image;
	image.setSize(width, height);
	GPlotWindow pw(&image, xmin, ymin, xmin + xrange, ymin + yrange);
	GTEMPBUF(double, features, pData->cols());
	double* labels = features + featureDims;
	unsigned int* pPix = image.pixels();
	size_t step = std::max((size_t)1, pData->rows() / 100);
	double xx, yy;
	for(int y = 0; y < height; y++)
	{
		cout << ((float)y * 100.0f / height) << "%       \r";
		cout.flush();
		for(int x = 0; x < width; x++)
		{
			pw.viewToWindow(x, y, &xx, &yy);
			size_t r = 0;
			size_t g = 0;
			size_t b = 0;
			size_t count = 0;
			for(size_t i = 0; i < pData->rows(); i += step)
			{
				GVec::copy(features, pData->row(i), featureDims);
				features[attrx] = xx;
				features[attry] = yy;
				pModeler->predict(features, labels);
				unsigned int hue = gAHSV(0xff, std::max(0.0f, std::min(1.0f, (float)((labels[labelDim] - labelMin) / labelRange))), 1.0f, 0.5f);
				r += gRed(hue);
				g += gGreen(hue);
				b += gBlue(hue);
				count++;
			}
			r /= count;
			g /= count;
			b /= count;
			*pPix = gARGB(0xff, ClipChan((int)r), ClipChan((int)g), ClipChan((int)b));
			pPix++;
		}
	}
	cout << "                \n";
	cout.flush();

	// Plot the data
	for(size_t i = 0; i < pData->rows(); i++)
	{
		double* pRow = pData->row(i);
		pw.dot(pRow[attrx], pRow[attry], dotRadius, gAHSV(0xff, std::max(0.0f, std::min(1.0f, (float)((pRow[featureDims + labelDim] - labelMin) / labelRange))), 1.0, 1.0), 0xff000000);
	}

	savePng(&image, filename.c_str());
	cout << "Output saved to " << filename.c_str() << ".\n";
}

void rayTraceManifoldModel(GArgReader& args)
{
	// Load the model
	GDom doc;
	if(args.size() < 1)
		throw Ex("Model not specified");
	doc.loadJson(args.pop_string());
	GRand prng(0);
	GLearnerLoader ll(prng, true);
	GSupervisedLearner* pModeler = ll.loadSupervisedLearner(doc.root());
	Holder<GSupervisedLearner> hModeler(pModeler);
	if(pModeler->relFeatures()->size() != 2 || pModeler->relLabels()->size() != 3)
		throw Ex("The model has ", to_str(pModeler->relFeatures()->size()), " inputs and ", to_str(pModeler->relLabels()->size()), " outputs. 2 real inputs and 3 real outputs are expected");

	// Parse options
	int width = 400;
	int height = 400;
	double amin = 0.0;
	double amax = 1.0;
	double bmin = 0.0;
	double bmax = 1.0;
/*	double xmin = 0.0;
	double xmax = 1.0;
	double ymin = 0.0;
	double ymax = 1.0;
	double zmin = 0.0;
	double zmax = 1.0;*/
	size_t granularity = 50;
	string filename = "plot.png";
	double pointRadius = 0.02;
	GMatrix* pPoints = NULL;
	Holder<GMatrix> hPoints;
	while(args.next_is_flag())
	{
		if(args.if_pop("-size"))
		{
			width = args.pop_uint();
			height = args.pop_uint();
		}
		else if(args.if_pop("-out"))
			filename = args.pop_string();
		else if(args.if_pop("-domain"))
		{
			amin = args.pop_double();
			amax = args.pop_double();
			bmin = args.pop_double();
			bmax = args.pop_double();
		}
/*		else if(args.if_pop("-range"))
		{
			xmin = args.pop_double();
			xmax = args.pop_double();
			ymin = args.pop_double();
			ymax = args.pop_double();
			zmin = args.pop_double();
			zmax = args.pop_double();
		}*/
		else if(args.if_pop("-points"))
		{
			delete(pPoints);
			pPoints = GMatrix::loadArff(args.pop_string());
			hPoints.reset(pPoints);
			if(pPoints->cols() != 3)
				throw Ex("Expected 3-dimensional points");
		}
		else if(args.if_pop("-pointradius"))
			pointRadius = args.pop_double();
		else if(args.if_pop("-granularity"))
			granularity = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Set up the scene
	GRayTraceScene scene(&prng);
	scene.setBackgroundColor(1.0, 0.0, 0.0, 0.0);
	scene.setAmbientLight(0.9, 0.9, 0.9);
	scene.addLight(new GRayTraceDirectionalLight(0.1, 0.2, 0.3, // direction
							0.8, 0.8, 1.0, // color
							0.0)); // jitter
	scene.addLight(new GRayTraceDirectionalLight(-0.1, -0.2, 0.3, // direction
							0.8, 1.0, 0.8, // color
							0.0)); // jitter
	scene.addLight(new GRayTraceDirectionalLight(-0.1, 0.9, -0.1, // direction
							0.3, 0.2, 0.2, // color
							0.0)); // jitter
	GRayTraceCamera* pCamera = scene.camera();
	pCamera->setImageSize(width, height);
	pCamera->setViewAngle(M_PI / 3);
	G3DVector mean;
	mean.set(0, 0, 0);


	G3DVector cameraDirection(-.35, -0.15, -0.5);
//	G3DVector cameraDirection(0.1, -0.25, -0.85);
	G3DReal dist = 2.0;
	G3DVector* pCameraPos = pCamera->lookFromPoint();
	pCameraPos->copy(&cameraDirection);
	pCameraPos->multiply(-1);
	pCameraPos->normalize();
	pCameraPos->multiply(dist);
	pCameraPos->add(&mean);
	pCamera->setDirection(&cameraDirection, 0.0);

	// Make bluish material
	GRayTracePhysicalMaterial* pMat1 = new GRayTracePhysicalMaterial();
	scene.addMaterial(pMat1);
	pMat1->setColor(GRayTraceMaterial::Diffuse, 0.3, 0.4, 0.6);
	pMat1->setColor(GRayTraceMaterial::Specular, 0.4, 0.4, 0.6);
	pMat1->setColor(GRayTraceMaterial::Reflective, 0.2, 0.2, 0.3);
	pMat1->setColor(GRayTraceMaterial::Transmissive, 0.7, 0.7, 0.8);

	// Make yellowish material
	GRayTracePhysicalMaterial* pMat2 = new GRayTracePhysicalMaterial();
	scene.addMaterial(pMat2);
	pMat2->setColor(GRayTraceMaterial::Diffuse, 0.4, 0.4, 0.05);
	pMat2->setColor(GRayTraceMaterial::Specular, 1.0, 1.0, 0.8);
	pMat2->setColor(GRayTraceMaterial::Reflective, 0.5, 0.5, 0.3);

	// Make the surface
	double in[2];
	double astep = (amax - amin) / (std::max((size_t)2, granularity) - 1);
	double bstep = (bmax - bmin) / (std::max((size_t)2, granularity) - 1);
	for(in[1] = bmin; in[1] + bstep <= bmax; in[1] += bstep)
	{
		for(in[0] = amin; in[0] + astep <= amax; )
		{
			// Predict the 4 corners
			G3DVector v1, v2, v3, v4;
			pModeler->predict(in, v1.vals());
			in[1] += bstep;
			pModeler->predict(in, v3.vals());
			in[0] += astep;
			pModeler->predict(in, v4.vals());
			in[1] -= bstep;
			pModeler->predict(in, v2.vals());

			// Add a quad surface
			scene.addMesh(GRayTraceTriMesh::makeQuadSurface(pMat1, &v1, &v3, &v4, &v2));
		}
	}

	// Make the points
	if(pPoints)
	{
		for(size_t i = 0; i < pPoints->rows(); i++)
		{
			double* pVec = pPoints->row(i);
			scene.addObject(new GRayTraceSphere(pMat2, pVec[0], pVec[1], pVec[2], pointRadius));
		}
	}

//	scene.addObject(new GRayTraceSphere(pMat2, .5,.5,.5, 0.02)); // xyzr

	// Ray-trace the scene
	scene.render();
	GImage* pImage = scene.image();
	savePng(pImage, filename.c_str());
	cout << "Output saved to " << filename.c_str() << ".\n";
}

void rowToImage(GArgReader& args)
{
	GMatrix* pData = loadData(args.pop_string());
	Holder<GMatrix> hData(pData);
	unsigned int r = args.pop_uint();
	if(r > pData->rows())
		throw Ex("row index out of range");
	unsigned int width = args.pop_uint();

	string filename = "plot.png";
	int channels = 3;
	double range = 255.0;

	size_t cols = pData->cols();
	if((cols % (channels * width)) != 0)
		throw Ex("The row has ", to_str(cols), " dims, which is not a multiple of ", to_str(channels), " channels times ", to_str(width), " pixels wide");
	double* pRow = pData->row(r);
	unsigned int height = (unsigned int)cols / (unsigned int)(channels * width);
	GImage image;
	GVec::toImage(pRow, &image, width, height, channels, range);
	savePng(&image, filename.c_str());
	cout << "Image saved to " << filename.c_str() << ".\n";
}

void systemFrames(GArgReader& args)
{
	GDom doc;
	doc.loadJson(args.pop_string());
	GMatrix* pActions = loadData(args.pop_string());
	Holder<GMatrix> hActions(pActions);
	GMatrix* pObs = NULL;
	Holder<GMatrix> hObs(NULL);

	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	bool calibrate = false;
	int frameWidth = 256;
	int stepsPerFrame = 1;
	double scalePredictions = 1.0;
	string outFilename = "frames.png";
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-calibrate"))
			calibrate = true;
		else if(args.if_pop("-framewidth"))
			frameWidth = args.pop_uint();
		else if(args.if_pop("-stepsperframe"))
			stepsPerFrame = args.pop_uint();
		else if(args.if_pop("-scalepredictions"))
			scalePredictions = args.pop_double();
		else if(args.if_pop("-out"))
			outFilename = args.pop_string();
		else if(args.if_pop("-observations"))
		{
			pObs = loadData(args.pop_string());
			hObs.reset(pObs);
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Instantiate the model
	GRand prng(seed);
	GRecurrentModel rm(doc.root(), &prng);
	GImage* pImage = rm.frames(pActions, pObs, calibrate, frameWidth, stepsPerFrame, scalePredictions);
	Holder<GImage> hImage(pImage);
	savePng(pImage, outFilename.c_str());
	cout << "Frames saved to " << outFilename.c_str() << ".\n";
}

void ubpFrames(GArgReader& args)
{
	const char* szModelFilename = args.pop_string();
	size_t imageWid = args.pop_uint();
	size_t imageHgt = args.pop_uint();
	size_t framesHoriz = args.pop_uint();
	size_t framesVert = args.pop_uint();
	const char* outFilename = args.pop_string();

	GDom doc;
	doc.loadJson(szModelFilename);
	GRand rand(0);
	GLearnerLoader ll(rand);
	GUnsupervisedBackProp* pUBP = new GUnsupervisedBackProp(doc.root(), ll);
	Holder<GUnsupervisedBackProp> hUBP(pUBP);

	size_t featureDims = pUBP->neuralNet()->relFeatures()->size() - 2;
	GTEMPBUF(double, pFeatures, featureDims);
	GVec::setAll(pFeatures, 0.5, featureDims);
	size_t labelDims = pUBP->labelDims();
	GTEMPBUF(double, pLabels, labelDims);
	GImage image;
	image.setSize((imageWid + 1) * framesHoriz, (imageHgt + 1) * framesVert);
	image.clear(0xff008000);
	size_t yy = 0;
	for(size_t vFrame = 0; vFrame < framesVert; vFrame++)
	{
		size_t xx = 0;
		for(size_t hFrame = 0; hFrame < framesHoriz; hFrame++)
		{
			pFeatures[featureDims - 2] = (double)hFrame / (framesHoriz - 1);
			pFeatures[featureDims - 1] = (double)vFrame / (framesVert - 1);
			pUBP->lowToHi(pFeatures, pLabels);
			GImage tmp;
			GVec::toImage(pLabels, &tmp, imageWid, imageHgt, pUBP->neuralNet()->relLabels()->size(), 255.0);
			image.blit(xx, yy, &tmp);
			xx += imageWid + 1;
		}
		yy += imageHgt + 1;
	}
	savePng(&image, outFilename);
}

void ShowUsage(const char* appName)
{
	cout << "Full Usage Information\n";
	cout << "[Square brackets] are used to indicate required arguments.\n";
	cout << "<Angled brackets> are used to indicate optional arguments.\n";
	cout << "\n";
	UsageNode* pUsageTree = makeGenerateUsageTree();
	Holder<UsageNode> hUsageTree(pUsageTree);
	pUsageTree->print(cout, 0, 3, 76, 1000, true);
	cout.flush();
}

void showError(GArgReader& args, const char* szAppName, const char* szMessage)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	args.set_pos(1);
	const char* szCommand = args.peek();
	UsageNode* pUsageTree = makeGenerateUsageTree();
	Holder<UsageNode> hUsageTree(pUsageTree);
	if(szCommand)
	{
		UsageNode* pUsageCommand = pUsageTree->choice(szCommand);
		if(pUsageCommand)
		{
			cerr << "Brief Usage Information:\n\n";
			cerr << szAppName << " ";
			pUsageCommand->print(cerr, 0, 3, 76, 1000, true);
			if(pUsageCommand->findPart("[neighbor-finder]") >= 0)
			{
				UsageNode* pNFTree = makeNeighborUsageTree();
				Holder<UsageNode> hNFTree(pNFTree);
				pNFTree->print(cerr, 1, 3, 76, 2, false);
			}
		}
		else
		{
			cerr << "Brief Usage Information:\n\n";
			pUsageTree->print(cerr, 0, 3, 76, 1, false);
		}
	}
	else
	{
		pUsageTree->print(cerr, 0, 3, 76, 1, false);
		cerr << "\nFor more specific usage information, enter as much of the command as you know.\n";
	}
	cerr << "\nTo see full usage information, run:\n	" << szAppName << " usage\n\n";
	cerr << "For a graphical tool that will help you to build a command, run:\n	waffles_wizard\n";
	cerr.flush();
}

int main(int argc, char *argv[])
{
	PathData pd;
	GFile::parsePath(argv[0], &pd);
	const char* appName = argv[0] + pd.fileStart;
	GArgReader args(argc, argv);
	args.pop_string();
	int ret = 0;
	try
	{
		if(args.size() < 1) throw Ex("Expected a command");
		else if(args.if_pop("3d")) Plot3dMulti(args);
		else if(args.if_pop("usage")) ShowUsage(appName);
		else if(args.if_pop("crane")) CraneDataset(args);
		else if(args.if_pop("cranepath")) cranePath(args);
		else if(args.if_pop("cube")) cube(args);
		else if(args.if_pop("datatoframes")) dataToFrames(args);
		else if(args.if_pop("entwinedspirals")) EntwinedSpirals(args);
		else if(args.if_pop("fishbowl")) fishBowl(args);
		else if(args.if_pop("gridrandomwalk")) gridRandomWalk(args);
		else if(args.if_pop("imagetranslatedovernoise")) ImageTranslatedOverNoise(args);
		else if(args.if_pop("manifold")) manifold(args);
		else if(args.if_pop("mechanicalrabbit")) mechanicalRabbit(args);
		else if(args.if_pop("model")) model(args);
		else if(args.if_pop("noise")) Noise(args);
		else if(args.if_pop("overview")) PlotCorrelations(args);
		else if(args.if_pop("ppmtopng")) ppmToPng(args);
		else if(args.if_pop("randomsequence")) randomSequence(args);
		else if(args.if_pop("raytracesurface")) rayTraceManifoldModel(args);
		else if(args.if_pop("rowtoimage")) rowToImage(args);
		else if(args.if_pop("scalerotate")) ScaleAndRotate(args);
		else if(args.if_pop("scenerobotsimulationgrid")) sceneRobotSimulationGrid(args);
		else if(args.if_pop("scenerobotsimulationpath")) sceneRobotSimulationPath(args);
		else if(args.if_pop("scurve")) SCurve(args);
		else if(args.if_pop("selfintersectingribbon")) SelfIntersectingRibbon(args);
		else if(args.if_pop("swissroll")) SwissRoll(args);
		else if(args.if_pop("systemframes")) systemFrames(args);
		else if(args.if_pop("threecranepath")) threeCranePath(args);
		else if(args.if_pop("ubpframes")) ubpFrames(args);
		else if(args.if_pop("vectortoimage")) vectorToImage(args);
		else if(args.if_pop("windowedimage")) WindowedImageData(args);
		else throw Ex("Unrecognized command: ", args.peek());
	}
	catch(const std::exception& e)
	{
		showError(args, appName, e.what());
		ret = 1;
	}

	return ret;
}

