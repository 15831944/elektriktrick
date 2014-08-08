//
//  ETWireframeModel.cpp
//  Electrictrick
//
//  Created by Matthias Melcher on 6/26/14.
//  Copyright (c) 2014 M.Melcher GmbH. All rights reserved.
//

#include "ETWireframeModel.h"
#include "ETEdge.h"
#include "ETVector.h"


ETWireframeModel::ETWireframeModel()
:   ETModel(),
    edge(0L),
    nEdge(0),
    NEdge(0),
    sortedEdge(0L)
{
}


ETWireframeModel::~ETWireframeModel()
{
    if (edge)
        free(edge);
    if (sortedEdge)
        free(sortedEdge);
}


/**
 * Draw all edges in the model starting at the furthest edge all the way to the closest one.
 */
void ETWireframeModel::Draw(void* ctx, int width, int height)
{
    CGContextRef cgContext = (CGContextRef)ctx;
    int xoff = width/2;
    int yoff = height/2;
    int xscl = height*0.42; // yes, that is "height", assuming that height is smaller than width
    int yscl = height*0.42;
    uint32_t i;
    for (i=0; i<nEdge; ++i) {
        ETEdge *e = sortedEdge[i];
        float lum = 0.7f - 0.4f * e->lum;
        float hue = 0.5f + 0.5f * e->hue;
        if (hue<0.0) hue = 0.0;
        if (hue>1.0) hue = 1.0;
        // I want a color range from red at the bottom to yellow at the top
        // and dark in the back to lighter in the front:
        // FF0000 to FFFF00
        CGContextSetRGBStrokeColor(cgContext, lum*1.0, lum*hue, lum*0.0, 1.0);
        CGContextBeginPath(cgContext);
        CGContextMoveToPoint(cgContext, e->p0.x*xscl+xoff, e->p0.y*yscl+yoff);
        CGContextAddLineToPoint(cgContext, e->p1.x*xscl+xoff, e->p1.y*yscl+yoff);
        CGContextClosePath(cgContext);
        CGContextDrawPath(cgContext, kCGPathStroke);
        // TODO: if (QLPreviewRequestIsCancelled(preview)) break;
    }
}


void ETWireframeModel::PrepareDrawing()
{
    // prepare the polygon data for rendering
    FindBoundingBox();
    FixupCoordinates();
    SimpleProjection();
    DepthSort();
}


/**
 * Rotate the model slightly around y and x to give an orthogonal view from the top right.
 */
int ETWireframeModel::SimpleProjection()
{
    uint32_t i;
    for (i=0; i<nEdge; ++i) {
        ETEdge *e = edge+i;
        SimpleProjection(e->p0);
        SimpleProjection(e->p1);
    }
    return 0;
}


int ETWireframeModel::FixupCoordinates()
{
    const float ry = 180.0+45.0; // deg
    float rys = sinf(ry/180.0*M_PI);
    float ryc = cosf(ry/180.0*M_PI);
    
    uint32_t i;
    
    // find object center and size
    dx = 0.5f*(pBBoxMax.x-pBBoxMin.x);
    dy = 0.5f*(pBBoxMax.y-pBBoxMin.y);
    dz = 0.5f*(pBBoxMax.z-pBBoxMin.z);
    cx = pBBoxMin.x + dx;
    cy = pBBoxMin.y + dy;
    cz = pBBoxMin.z + dz;
    
    // normalize the object size and position to fit into a -1 to 1 cube
    float scl = dx; if (dy>scl) scl = dy; if (dz>scl) scl = dz;
    if (scl>0.0) scl = 1.0f/scl;
    for (i=0; i<nEdge; ++i) {
        ETEdge *e = edge+i;
        e->hue = 0.5f * (e->p0.z-cz + e->p1.z-cz) / dz;
        e->lum = 0.5f * (-rys*(e->p0.y-cy + e->p1.y-cy)/dy + ryc*(e->p0.x-cx + e->p1.x-cx)/dx);
        e->p0.x = (e->p0.x - cx) * scl;
        e->p0.y = (e->p0.y - cy) * scl;
        e->p0.z = (e->p0.z - cz) * scl;
        e->p1.x = (e->p1.x - cx) * scl;
        e->p1.y = (e->p1.y - cy) * scl;
        e->p1.z = (e->p1.z - cz) * scl;
    }
    return 0;
}


/**
 * Find the bounding box and reposition coordinates to fit into a -1/1 box.
 */
void ETWireframeModel::FindBoundingBox()
{
    uint32_t i;
    float minX=0, minY=0, minZ=0, maxX=0, maxY=0, maxZ=0;
    
    for (i=0; i<nEdge; ++i) {
        ETEdge *e = edge+i;
        if (i==0) {
            minX = maxX = e->p0.x;
            minY = maxY = e->p0.y;
            minZ = maxZ = e->p0.z;
        } else {
            // update the boundign box
            if (e->p0.x<minX) minX = e->p0.x; if (e->p0.x>maxX) maxX = e->p0.x;
            if (e->p0.y<minY) minY = e->p0.y; if (e->p0.y>maxY) maxY = e->p0.y;
            if (e->p0.z<minZ) minZ = e->p0.z; if (e->p0.z>maxZ) maxZ = e->p0.z;
            if (e->p1.x<minX) minX = e->p1.x; if (e->p1.x>maxX) maxX = e->p1.x;
            if (e->p1.y<minY) minY = e->p1.y; if (e->p1.y>maxY) maxY = e->p1.y;
            if (e->p1.z<minZ) minZ = e->p1.z; if (e->p1.z>maxZ) maxZ = e->p1.z;
        }
    }
    pBBoxMin.set(minX, minY, minZ);
    pBBoxMax.set(maxX, maxY, maxZ);
}


/**
 * Sort all edges in Z depth to allow rendering with an illusion of depth.
 *
 * This function creates an array of pointers to the original edge data.
 */
int ETWireframeModel::DepthSort()
{
    uint32_t i, j=0;
    sortedEdge = (ETEdge**)calloc(nEdge, sizeof(ETEdge*));
    for (i=0; i<nEdge; i++) {
        ETEdge *e = edge+i;
        sortedEdge[j++] = e;
        // prepare for depth sorting
        e->z = (e->p0.z + e->p1.z) / 2.0;
    }
    qsort(sortedEdge, nEdge, sizeof(ETEdge*), CompareEdgeZ);
    return 0;
}


/**
 * Compare the z position of edges and retrun -1, 0, or 1 for close, equal, and further.
 *
 * This is just a rough approximation.
 */
int ETWireframeModel::CompareEdgeZ(const void *a, const void *b)
{
    const ETEdge* ta = *(const ETEdge**)a;
    const ETEdge* tb = *(const ETEdge**)b;
    
    // simple mid point depth comparison
    int v = (ta->z < tb->z) - (ta->z > tb->z);
    return v;
}



