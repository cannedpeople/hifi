//
//  GeometryCache.cpp
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 6/21/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// include this before QOpenGLBuffer, which includes an earlier version of OpenGL
#include <gpu/GPUConfig.h>

#include <cmath>

#include <QNetworkReply>
#include <QRunnable>
#include <QThreadPool>

#include <SharedUtil.h>

#include "TextureCache.h"
#include "GeometryCache.h"

//#define WANT_DEBUG

const int GeometryCache::UNKNOWN_ID = -1;

GeometryCache::GeometryCache() :
    _nextID(0)
{
    const qint64 GEOMETRY_DEFAULT_UNUSED_MAX_SIZE = DEFAULT_UNUSED_MAX_SIZE;
    setUnusedResourceCacheSize(GEOMETRY_DEFAULT_UNUSED_MAX_SIZE);
}

GeometryCache::~GeometryCache() {
    foreach (const VerticesIndices& vbo, _hemisphereVBOs) {
        glDeleteBuffers(1, &vbo.first);
        glDeleteBuffers(1, &vbo.second);
    }
}

void GeometryCache::renderHemisphere(int slices, int stacks) {
    VerticesIndices& vbo = _hemisphereVBOs[IntPair(slices, stacks)];
    int vertices = slices * (stacks - 1) + 1;
    int indices = slices * 2 * 3 * (stacks - 2) + slices * 3;
    if (vbo.first == 0) {    
        GLfloat* vertexData = new GLfloat[vertices * 3];
        GLfloat* vertex = vertexData;
        for (int i = 0; i < stacks - 1; i++) {
            float phi = PI_OVER_TWO * (float)i / (float)(stacks - 1);
            float z = sinf(phi), radius = cosf(phi);
            
            for (int j = 0; j < slices; j++) {
                float theta = TWO_PI * (float)j / (float)slices;

                *(vertex++) = sinf(theta) * radius;
                *(vertex++) = cosf(theta) * radius;
                *(vertex++) = z;
            }
        }
        *(vertex++) = 0.0f;
        *(vertex++) = 0.0f;
        *(vertex++) = 1.0f;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        const int BYTES_PER_VERTEX = 3 * sizeof(GLfloat);
        glBufferData(GL_ARRAY_BUFFER, vertices * BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < stacks - 2; i++) {
            GLushort bottom = i * slices;
            GLushort top = bottom + slices;
            for (int j = 0; j < slices; j++) {
                int next = (j + 1) % slices;
                
                *(index++) = bottom + j;
                *(index++) = top + next;
                *(index++) = top + j;
                
                *(index++) = bottom + j;
                *(index++) = bottom + next;
                *(index++) = top + next;
            }
        }
        GLushort bottom = (stacks - 2) * slices;
        GLushort top = bottom + slices;
        for (int i = 0; i < slices; i++) {    
            *(index++) = bottom + i;
            *(index++) = bottom + (i + 1) % slices;
            *(index++) = top;
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        const int BYTES_PER_INDEX = sizeof(GLushort);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
 
    glVertexPointer(3, GL_FLOAT, 0, 0);
    glNormalPointer(GL_FLOAT, 0, 0);
        
    glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
        
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

const int NUM_VERTICES_PER_TRIANGLE = 3;
const int NUM_TRIANGLES_PER_QUAD = 2;
const int NUM_VERTICES_PER_TRIANGULATED_QUAD = NUM_VERTICES_PER_TRIANGLE * NUM_TRIANGLES_PER_QUAD;
const int NUM_COORDS_PER_VERTEX = 3;
const int NUM_BYTES_PER_VERTEX = NUM_COORDS_PER_VERTEX * sizeof(GLfloat);
const int NUM_BYTES_PER_INDEX = sizeof(GLushort);

void GeometryCache::renderSphere(float radius, int slices, int stacks, bool solid) {
    VerticesIndices& vbo = _sphereVBOs[IntPair(slices, stacks)];
    int vertices = slices * (stacks - 1) + 2;    
    int indices = slices * stacks * NUM_VERTICES_PER_TRIANGULATED_QUAD;
    if (vbo.first == 0) {        
        GLfloat* vertexData = new GLfloat[vertices * NUM_COORDS_PER_VERTEX];
        GLfloat* vertex = vertexData;

        // south pole
        *(vertex++) = 0.0f;
        *(vertex++) = 0.0f;
        *(vertex++) = -1.0f;

        //add stacks vertices climbing up Y axis
        for (int i = 1; i < stacks; i++) {
            float phi = PI * (float)i / (float)(stacks) - PI_OVER_TWO;
            float z = sinf(phi), radius = cosf(phi);
            
            for (int j = 0; j < slices; j++) {
                float theta = TWO_PI * (float)j / (float)slices;

                *(vertex++) = sinf(theta) * radius;
                *(vertex++) = cosf(theta) * radius;
                *(vertex++) = z;
            }
        }

        // north pole
        *(vertex++) = 0.0f;
        *(vertex++) = 0.0f;
        *(vertex++) = 1.0f;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;

        // South cap
        GLushort bottom = 0;
        GLushort top = 1;
        for (int i = 0; i < slices; i++) {    
            *(index++) = bottom;
            *(index++) = top + i;
            *(index++) = top + (i + 1) % slices;
        }

        // (stacks - 2) ribbons
        for (int i = 0; i < stacks - 2; i++) {
            bottom = i * slices + 1;
            top = bottom + slices;
            for (int j = 0; j < slices; j++) {
                int next = (j + 1) % slices;
                
                *(index++) = top + next;
                *(index++) = bottom + j;
                *(index++) = top + j;
                
                *(index++) = bottom + next;
                *(index++) = bottom + j;
                *(index++) = top + next;
            }
        }
        
        // north cap
        bottom = (stacks - 2) * slices + 1;
        top = bottom + slices;
        for (int i = 0; i < slices; i++) {    
            *(index++) = bottom + (i + 1) % slices;
            *(index++) = bottom + i;
            *(index++) = top;
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, 0);
    glNormalPointer(GL_FLOAT, 0, 0);

    glPushMatrix();
    glScalef(radius, radius, radius);
    if (solid) {
        glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    } else {
        glDrawRangeElementsEXT(GL_LINES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    }
    glPopMatrix();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderSquare(int xDivisions, int yDivisions) {
    VerticesIndices& vbo = _squareVBOs[IntPair(xDivisions, yDivisions)];
    int xVertices = xDivisions + 1;
    int yVertices = yDivisions + 1;
    int vertices = xVertices * yVertices;
    int indices = 2 * 3 * xDivisions * yDivisions;
    if (vbo.first == 0) {
        GLfloat* vertexData = new GLfloat[vertices * 3];
        GLfloat* vertex = vertexData;
        for (int i = 0; i <= yDivisions; i++) {
            float y = (float)i / yDivisions;
            
            for (int j = 0; j <= xDivisions; j++) {
                *(vertex++) = (float)j / xDivisions;
                *(vertex++) = y;
                *(vertex++) = 0.0f;
            }
        }
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < yDivisions; i++) {
            GLushort bottom = i * xVertices;
            GLushort top = bottom + xVertices;
            for (int j = 0; j < xDivisions; j++) {
                int next = j + 1;
                
                *(index++) = bottom + j;
                *(index++) = top + next;
                *(index++) = top + j;
                
                *(index++) = bottom + j;
                *(index++) = bottom + next;
                *(index++) = top + next;
            }
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
 
    // all vertices have the same normal
    glNormal3f(0.0f, 0.0f, 1.0f);
    
    glVertexPointer(3, GL_FLOAT, 0, 0);
        
    glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
        
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderHalfCylinder(int slices, int stacks) {
    VerticesIndices& vbo = _halfCylinderVBOs[IntPair(slices, stacks)];
    int vertices = (slices + 1) * stacks;
    int indices = 2 * 3 * slices * (stacks - 1);
    if (vbo.first == 0) {    
        GLfloat* vertexData = new GLfloat[vertices * 2 * 3];
        GLfloat* vertex = vertexData;
        for (int i = 0; i <= (stacks - 1); i++) {
            float y = (float)i / (stacks - 1);
            
            for (int j = 0; j <= slices; j++) {
                float theta = 3.0f * PI_OVER_TWO + PI * (float)j / (float)slices;

                //normals
                *(vertex++) = sinf(theta);
                *(vertex++) = 0;
                *(vertex++) = cosf(theta);

                // vertices
                *(vertex++) = sinf(theta);
                *(vertex++) = y;
                *(vertex++) = cosf(theta);
            }
        }
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, 2 * vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < stacks - 1; i++) {
            GLushort bottom = i * (slices + 1);
            GLushort top = bottom + slices + 1;
            for (int j = 0; j < slices; j++) {
                int next = j + 1;
                
                *(index++) = bottom + j;
                *(index++) = top + next;
                *(index++) = top + j;
                
                *(index++) = bottom + j;
                *(index++) = bottom + next;
                *(index++) = top + next;
            }
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glNormalPointer(GL_FLOAT, 6 * sizeof(float), 0);
    glVertexPointer(3, GL_FLOAT, (6 * sizeof(float)), (const void *)(3 * sizeof(float)));
        
    glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
        
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderCone(float base, float height, int slices, int stacks) {
    VerticesIndices& vbo = _halfCylinderVBOs[IntPair(slices, stacks)];
    int vertices = (stacks + 2) * slices;
    int baseTriangles = slices - 2;
    int indices = NUM_VERTICES_PER_TRIANGULATED_QUAD * slices * stacks + NUM_VERTICES_PER_TRIANGLE * baseTriangles;
    if (vbo.first == 0) {
        GLfloat* vertexData = new GLfloat[vertices * NUM_COORDS_PER_VERTEX * 2];
        GLfloat* vertex = vertexData;
        // cap
        for (int i = 0; i < slices; i++) {
            float theta = TWO_PI * i / slices;
            
            //normals
            *(vertex++) = 0.0f;
            *(vertex++) = 0.0f;
            *(vertex++) = -1.0f;

            // vertices
            *(vertex++) = cosf(theta);
            *(vertex++) = sinf(theta);
            *(vertex++) = 0.0f;
        }
        // body
        for (int i = 0; i <= stacks; i++) {
            float z = (float)i / stacks;
            float radius = 1.0f - z;
            
            for (int j = 0; j < slices; j++) {
                float theta = TWO_PI * j / slices;

                //normals
                *(vertex++) = cosf(theta) / SQUARE_ROOT_OF_2;
                *(vertex++) = sinf(theta) / SQUARE_ROOT_OF_2;
                *(vertex++) = 1.0f / SQUARE_ROOT_OF_2;

                // vertices
                *(vertex++) = radius * cosf(theta);
                *(vertex++) = radius * sinf(theta);
                *(vertex++) = z;
            }
        }
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, 2 * vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < baseTriangles; i++) {
            *(index++) = 0;
            *(index++) = i + 2;
            *(index++) = i + 1;
        }
        for (int i = 1; i <= stacks; i++) {
            GLushort bottom = i * slices;
            GLushort top = bottom + slices;
            for (int j = 0; j < slices; j++) {
                int next = (j + 1) % slices;
                
                *(index++) = bottom + j;
                *(index++) = top + next;
                *(index++) = top + j;
                
                *(index++) = bottom + j;
                *(index++) = bottom + next;
                *(index++) = top + next;
            }
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    int stride = NUM_VERTICES_PER_TRIANGULATED_QUAD * sizeof(float);
    glNormalPointer(GL_FLOAT, stride, 0);
    glVertexPointer(NUM_COORDS_PER_VERTEX, GL_FLOAT, stride, (const void *)(NUM_COORDS_PER_VERTEX * sizeof(float)));
    
    glPushMatrix();
    glScalef(base, base, height);
    
    glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    
    glPopMatrix();
    
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderGrid(int xDivisions, int yDivisions) {
    QOpenGLBuffer& buffer = _gridBuffers[IntPair(xDivisions, yDivisions)];
    int vertices = (xDivisions + 1 + yDivisions + 1) * 2;
    if (!buffer.isCreated()) {
        GLfloat* vertexData = new GLfloat[vertices * 2];
        GLfloat* vertex = vertexData;
        for (int i = 0; i <= xDivisions; i++) {
            float x = (float)i / xDivisions;
        
            *(vertex++) = x;
            *(vertex++) = 0.0f;
            
            *(vertex++) = x;
            *(vertex++) = 1.0f;
        }
        for (int i = 0; i <= yDivisions; i++) {
            float y = (float)i / yDivisions;
            
            *(vertex++) = 0.0f;
            *(vertex++) = y;
            
            *(vertex++) = 1.0f;
            *(vertex++) = y;
        }
        buffer.create();
        buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        buffer.bind();
        buffer.allocate(vertexData, vertices * 2 * sizeof(GLfloat));
        delete[] vertexData;
        
    } else {
        buffer.bind();
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    
    glVertexPointer(2, GL_FLOAT, 0, 0);
    
    glDrawArrays(GL_LINES, 0, vertices);
    
    glDisableClientState(GL_VERTEX_ARRAY);
    
    buffer.release();
}

void GeometryCache::renderGrid(int x, int y, int width, int height, int rows, int cols, int id) {
    bool registered = (id != UNKNOWN_ID);
    Vec3Pair key(glm::vec3(x, y, width), glm::vec3(height, rows, cols));
    QOpenGLBuffer& buffer = registered ? _registeredAlternateGridBuffers[id] : _alternateGridBuffers[key];

    // if this is a registered , and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && buffer.isCreated()) {
        Vec3Pair& lastKey = _lastRegisteredGrid[id];
        if (lastKey != key) {
            buffer.destroy();
            #ifdef WANT_DEBUG
                qDebug() << "renderGrid()... RELEASING REGISTERED";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderGrid()... REUSING PREVIOUSLY REGISTERED";
        }
        #endif // def WANT_DEBUG
    }

    int vertices = (cols + 1 + rows + 1) * 2;
    if (!buffer.isCreated()) {
    
        _lastRegisteredGrid[id] = key;
        
        GLfloat* vertexData = new GLfloat[vertices * 2];
        GLfloat* vertex = vertexData;

        int dx = width / cols;
        int dy = height / rows;
        int tx = x;
        int ty = y;

        // Draw horizontal grid lines
        for (int i = rows + 1; --i >= 0; ) {
            *(vertex++) = x;
            *(vertex++) = ty;

            *(vertex++) = x + width;
            *(vertex++) = ty;

            ty += dy;
        }
        // Draw vertical grid lines
        for (int i = cols + 1; --i >= 0; ) {
            //glVertex2i(tx, y);
            //glVertex2i(tx, y + height);
            *(vertex++) = tx;
            *(vertex++) = y;

            *(vertex++) = tx;
            *(vertex++) = y + height;
            tx += dx;
        }

        buffer.create();
        buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        buffer.bind();
        buffer.allocate(vertexData, vertices * 2 * sizeof(GLfloat));
        delete[] vertexData;

        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new grid buffer made -- _alternateGridBuffers.size():" << _alternateGridBuffers.size();
            } else {
                qDebug() << "new registered grid buffer made -- _registeredAlternateGridBuffers.size():" << _registeredAlternateGridBuffers.size();
            }
        #endif
        
    } else {
        buffer.bind();
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    
    glVertexPointer(2, GL_FLOAT, 0, 0);
    
    glDrawArrays(GL_LINES, 0, vertices);
    
    glDisableClientState(GL_VERTEX_ARRAY);
    
    buffer.release();
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec2>& points) {
    BufferDetails& details = _registeredVertices[id];

    if (details.buffer.isCreated()) {
        details.buffer.destroy();
        #ifdef WANT_DEBUG
            qDebug() << "updateVertices()... RELEASING REGISTERED";
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2;
    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    GLfloat* vertexData = new GLfloat[details.vertices * FLOATS_PER_VERTEX];
    GLfloat* vertex = vertexData;

    foreach (const glm::vec2& point, points) {
        *(vertex++) = point.x;
        *(vertex++) = point.y;
    }

    details.buffer.create();
    details.buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    details.buffer.bind();
    details.buffer.allocate(vertexData, details.vertices * FLOATS_PER_VERTEX * sizeof(GLfloat));
    delete[] vertexData;

    #ifdef WANT_DEBUG
        qDebug() << "new registered vertices buffer made -- _registeredVertices.size():" << _registeredVertices.size();
    #endif
}

void GeometryCache::updateVertices(int id, const QVector<glm::vec3>& points) {
    BufferDetails& details = _registeredVertices[id];

    if (details.buffer.isCreated()) {
        details.buffer.destroy();
        #ifdef WANT_DEBUG
            qDebug() << "updateVertices()... RELEASING REGISTERED";
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3;
    details.vertices = points.size();
    details.vertexSize = FLOATS_PER_VERTEX;

    GLfloat* vertexData = new GLfloat[details.vertices * FLOATS_PER_VERTEX];
    GLfloat* vertex = vertexData;

    foreach (const glm::vec3& point, points) {
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = point.z;
    }

    details.buffer.create();
    details.buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    details.buffer.bind();
    details.buffer.allocate(vertexData, details.vertices * FLOATS_PER_VERTEX * sizeof(GLfloat));
    delete[] vertexData;

    #ifdef WANT_DEBUG
        qDebug() << "new registered linestrip buffer made -- _registeredVertices.size():" << _registeredVertices.size();
    #endif
}

void GeometryCache::renderVertices(GLenum mode, int id) {
    BufferDetails& details = _registeredVertices[id];
    if (details.buffer.isCreated()) {
        details.buffer.bind();
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(details.vertexSize, GL_FLOAT, 0, 0);
        glDrawArrays(mode, 0, details.vertices);
        glDisableClientState(GL_VERTEX_ARRAY);
        details.buffer.release();
    }
}

void GeometryCache::renderSolidCube(float size) {
    VerticesIndices& vbo = _solidCubeVBOs[size];
    const int FLOATS_PER_VERTEX = 3;
    const int VERTICES_PER_FACE = 4;
    const int NUMBER_OF_FACES = 6;
    const int TRIANGLES_PER_FACE = 2;
    const int VERTICES_PER_TRIANGLE = 3;
    const int vertices = NUMBER_OF_FACES * VERTICES_PER_FACE * FLOATS_PER_VERTEX;
    const int indices = NUMBER_OF_FACES * TRIANGLES_PER_FACE * VERTICES_PER_TRIANGLE;
    const int vertexPoints = vertices * FLOATS_PER_VERTEX;
    if (vbo.first == 0) {
        GLfloat* vertexData = new GLfloat[vertexPoints * 2]; // vertices and normals
        GLfloat* vertex = vertexData;
        float halfSize = size / 2.0f;
        
        static GLfloat cannonicalVertices[vertexPoints] = 
                                    { 1, 1, 1,  -1, 1, 1,  -1,-1, 1,   1,-1, 1,   // v0,v1,v2,v3 (front)
                                      1, 1, 1,   1,-1, 1,   1,-1,-1,   1, 1,-1,   // v0,v3,v4,v5 (right)
                                      1, 1, 1,   1, 1,-1,  -1, 1,-1,  -1, 1, 1,   // v0,v5,v6,v1 (top)
                                     -1, 1, 1,  -1, 1,-1,  -1,-1,-1,  -1,-1, 1,   // v1,v6,v7,v2 (left)
                                     -1,-1,-1,   1,-1,-1,   1,-1, 1,  -1,-1, 1,   // v7,v4,v3,v2 (bottom)
                                      1,-1,-1,  -1,-1,-1,  -1, 1,-1,   1, 1,-1 }; // v4,v7,v6,v5 (back)

        // normal array
        static GLfloat cannonicalNormals[vertexPoints]  = 
                                  { 0, 0, 1,   0, 0, 1,   0, 0, 1,   0, 0, 1,   // v0,v1,v2,v3 (front)
                                    1, 0, 0,   1, 0, 0,   1, 0, 0,   1, 0, 0,   // v0,v3,v4,v5 (right)
                                    0, 1, 0,   0, 1, 0,   0, 1, 0,   0, 1, 0,   // v0,v5,v6,v1 (top)
                                   -1, 0, 0,  -1, 0, 0,  -1, 0, 0,  -1, 0, 0,   // v1,v6,v7,v2 (left)
                                    0,-1, 0,   0,-1, 0,   0,-1, 0,   0,-1, 0,   // v7,v4,v3,v2 (bottom)
                                    0, 0,-1,   0, 0,-1,   0, 0,-1,   0, 0,-1 }; // v4,v7,v6,v5 (back)

        // index array of vertex array for glDrawElements() & glDrawRangeElement()
        static GLubyte cannonicalIndices[indices]  = 
                                    { 0, 1, 2,   2, 3, 0,      // front
                                      4, 5, 6,   6, 7, 4,      // right
                                      8, 9,10,  10,11, 8,      // top
                                     12,13,14,  14,15,12,      // left
                                     16,17,18,  18,19,16,      // bottom
                                     20,21,22,  22,23,20 };    // back
        


        GLfloat* cannonicalVertex = &cannonicalVertices[0];
        GLfloat* cannonicalNormal = &cannonicalNormals[0];

        for (int i = 0; i < vertices; i++) {
            //normals
            *(vertex++) = *cannonicalNormal++;
            *(vertex++) = *cannonicalNormal++;
            *(vertex++) = *cannonicalNormal++;

            // vertices
            *(vertex++) = halfSize * *cannonicalVertex++;
            *(vertex++) = halfSize * *cannonicalVertex++;
            *(vertex++) = halfSize * *cannonicalVertex++;

        }
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glNormalPointer(GL_FLOAT, 6 * sizeof(float), 0);
    glVertexPointer(3, GL_FLOAT, (6 * sizeof(float)), (const void *)(3 * sizeof(float)));
        
    glDrawRangeElementsEXT(GL_TRIANGLES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
        
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderWireCube(float size) {
    VerticesIndices& vbo = _wireCubeVBOs[size];
    const int FLOATS_PER_VERTEX = 3;
    const int VERTICES_PER_EDGE = 2;
    const int TOP_EDGES = 4;
    const int BOTTOM_EDGES = 4;
    const int SIDE_EDGES = 4;
    const int vertices = 8;
    const int indices = (TOP_EDGES + BOTTOM_EDGES + SIDE_EDGES) * VERTICES_PER_EDGE;
    if (vbo.first == 0) {    
        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices, no normals because we're a wire cube
        GLfloat* vertex = vertexData;
        float halfSize = size / 2.0f;
        
        static GLfloat cannonicalVertices[] = 
                                    { 1, 1, 1,   1, 1,-1,  -1, 1,-1,  -1, 1, 1,   // v0, v1, v2, v3 (top)
                                      1,-1, 1,   1,-1,-1,  -1,-1,-1,  -1,-1, 1    // v4, v5, v6, v7 (bottom)
                                    };

        // index array of vertex array for glDrawRangeElement() as a GL_LINES for each edge
        static GLubyte cannonicalIndices[indices]  = { 
                                      0, 1,  1, 2,  2, 3,  3, 0, // (top)
                                      4, 5,  5, 6,  6, 7,  7, 4, // (bottom)
                                      0, 4,  1, 5,  2, 6,  3, 7, // (side edges)
                                    };
        
        for (int i = 0; i < vertexPoints; i++) {
            vertex[i] = cannonicalVertices[i] * halfSize;
        }
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_LINES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderBevelCornersRect(int x, int y, int width, int height, int bevelDistance, int id) {
    bool registeredRect = (id != UNKNOWN_ID);
    Vec3Pair key(glm::vec3(x, y, 0.0f), glm::vec3(width, height, bevelDistance));
    VerticesIndices& vbo = registeredRect ? _registeredRectVBOs[id] : _rectVBOs[key];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredRect && vbo.first != 0) {
        Vec3Pair& lastKey = _lastRegisteredRect[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderBevelCornersRect()... RELEASING REGISTERED RECT";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderBevelCornersRect()... REUSING PREVIOUSLY REGISTERED RECT";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2;
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 8;
    const int indices = 8;
    if (vbo.first == 0) {
        _lastRegisteredRect[id] = key;  

        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices, no normals because we're a 2D quad
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1, 2, 3, 4, 5, 6, 7};
        
        int vertexPoint = 0;

        // left side
        vertex[vertexPoint++] = x;
        vertex[vertexPoint++] = y + bevelDistance;
        
        vertex[vertexPoint++] = x;
        vertex[vertexPoint++] = y + height - bevelDistance;

        // top side
        vertex[vertexPoint++] = x + bevelDistance;
        vertex[vertexPoint++] = y + height;
        
        vertex[vertexPoint++] = x + width - bevelDistance;
        vertex[vertexPoint++] = y + height;

        // right
        vertex[vertexPoint++] = x + width;
        vertex[vertexPoint++] = y + height - bevelDistance;
        vertex[vertexPoint++] = x + width;
        vertex[vertexPoint++] = y + bevelDistance;

        // bottom
        vertex[vertexPoint++] = x + width - bevelDistance;
        vertex[vertexPoint++] = y;
        vertex[vertexPoint++] = x +bevelDistance;
        vertex[vertexPoint++] = y;

        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new rect VBO made -- _rectVBOs.size():" << _rectVBOs.size();
            } else {
                qDebug() << "new registered rect VBO made -- _registeredRectVBOs.size():" << _registeredRectVBOs.size();
            }
        #endif
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_POLYGON, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderQuad(const glm::vec2& minCorner, const glm::vec2& maxCorner, int id) {

    bool registeredQuad = (id != UNKNOWN_ID);
    Vec2Pair key(minCorner, maxCorner);
    VerticesIndices& vbo = registeredQuad ? _registeredQuadVBOs[id] : _quad2DVBOs[key];
    
    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredQuad && vbo.first != 0) {
        Vec2Pair& lastKey = _lastRegisteredQuad2D[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderQuad() vec2... RELEASING REGISTERED QUAD";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderQuad() vec2... REUSING PREVIOUSLY REGISTERED QUAD";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2;
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 4;
    const int indices = 4;
    if (vbo.first == 0) {
        _lastRegisteredQuad2D[id] = key;  
        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices, no normals because we're a 2D quad
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1, 2, 3};

        vertex[0] = minCorner.x;
        vertex[1] = minCorner.y;
        vertex[2] = maxCorner.x;
        vertex[3] = minCorner.y;
        vertex[4] = maxCorner.x;
        vertex[5] = maxCorner.y;
        vertex[6] = minCorner.x;
        vertex[7] = maxCorner.y;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new quad VBO made -- _quad2DVBOs.size():" << _quad2DVBOs.size();
            } else {
                qDebug() << "new registered quad VBO made -- _registeredQuadVBOs.size():" << _registeredQuadVBOs.size();
            }
        #endif
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_QUADS, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void GeometryCache::renderQuad(const glm::vec2& minCorner, const glm::vec2& maxCorner,
                    const glm::vec2& texCoordMinCorner, const glm::vec2& texCoordMaxCorner, int id) {

    bool registeredQuad = (id != UNKNOWN_ID);
    Vec2PairPair key(Vec2Pair(minCorner, maxCorner), Vec2Pair(texCoordMinCorner, texCoordMaxCorner));
    VerticesIndices& vbo = registeredQuad ? _registeredQuadVBOs[id] : _quad2DTextureVBOs[key];
    
    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredQuad && vbo.first != 0) {
        Vec2PairPair& lastKey = _lastRegisteredQuad2DTexture[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderQuad() vec2 + texture... RELEASING REGISTERED QUAD";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderQuad()  vec2 + texture... REUSING PREVIOUSLY REGISTERED QUAD";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2 * 2; // text coords & vertices
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 4;
    const int indices = 4;
    if (vbo.first == 0) {
        _lastRegisteredQuad2DTexture[id] = key;
        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // text coords & vertices
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1, 2, 3};
        int v = 0;

        vertex[v++] = minCorner.x;
        vertex[v++] = minCorner.y;
        vertex[v++] = texCoordMinCorner.x;
        vertex[v++] = texCoordMinCorner.y;
        
        vertex[v++] = maxCorner.x;
        vertex[v++] = minCorner.y;
        vertex[v++] = texCoordMaxCorner.x;
        vertex[v++] = texCoordMinCorner.y;
        
        vertex[v++] = maxCorner.x;
        vertex[v++] = maxCorner.y;
        vertex[v++] = texCoordMaxCorner.x;
        vertex[v++] = texCoordMaxCorner.y;
        
        vertex[v++] = minCorner.x;
        vertex[v++] = maxCorner.y;
        vertex[v++] = texCoordMinCorner.x;
        vertex[v++] = texCoordMaxCorner.y;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
       
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new quad + texture VBO made -- _quad2DTextureVBOs.size():" << _quad2DTextureVBOs.size();
            } else {
                qDebug() << "new registered quad VBO made -- _registeredQuadVBOs.size():" << _registeredQuadVBOs.size();
            }
        #endif
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, NUM_BYTES_PER_VERTEX, 0);
    glTexCoordPointer(2, GL_FLOAT, NUM_BYTES_PER_VERTEX, (const void *)(2 * sizeof(float)));

    glDrawRangeElementsEXT(GL_QUADS, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderQuad(const glm::vec3& minCorner, const glm::vec3& maxCorner, int id) {

    bool registeredQuad = (id != UNKNOWN_ID);
    Vec3Pair key(minCorner, maxCorner);
    VerticesIndices& vbo = registeredQuad ? _registeredQuadVBOs[id] : _quad3DVBOs[key];
    
    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredQuad && vbo.first != 0) {
        Vec3Pair& lastKey = _lastRegisteredQuad3D[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderQuad() vec3... RELEASING REGISTERED QUAD";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderQuad()  vec3... REUSING PREVIOUSLY REGISTERED QUAD";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3;
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 4;
    const int indices = 4;
    if (vbo.first == 0) {    
        _lastRegisteredQuad3D[id] = key;
        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1, 2, 3};
        int v = 0;

        vertex[v++] = minCorner.x;
        vertex[v++] = minCorner.y;
        vertex[v++] = minCorner.z;

        vertex[v++] = maxCorner.x;
        vertex[v++] = minCorner.y;
        vertex[v++] = minCorner.z;

        vertex[v++] = maxCorner.x;
        vertex[v++] = maxCorner.y;
        vertex[v++] = maxCorner.z;

        vertex[v++] = minCorner.x;
        vertex[v++] = maxCorner.y;
        vertex[v++] = maxCorner.z;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new quad VBO made -- _quad3DVBOs.size():" << _quad3DVBOs.size();
            } else {
                qDebug() << "new registered quad VBO made -- _registeredQuadVBOs.size():" << _registeredQuadVBOs.size();
            }
        #endif
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_QUADS, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void GeometryCache::renderQuad(const glm::vec3& topLeft, const glm::vec3& bottomLeft, 
                    const glm::vec3& bottomRight, const glm::vec3& topRight,
                    const glm::vec2& texCoordTopLeft, const glm::vec2& texCoordBottomLeft,
                    const glm::vec2& texCoordBottomRight, const glm::vec2& texCoordTopRight, int id) {

    #ifdef WANT_DEBUG
        qDebug() << "renderQuad() vec3 + texture VBO...";
        qDebug() << "    topLeft:" << topLeft;
        qDebug() << "    bottomLeft:" << bottomLeft;
        qDebug() << "    bottomRight:" << bottomRight;
        qDebug() << "    topRight:" << topRight;
        qDebug() << "    texCoordTopLeft:" << texCoordTopLeft;
        qDebug() << "    texCoordBottomRight:" << texCoordBottomRight;
    #endif //def WANT_DEBUG
    
    bool registeredQuad = (id != UNKNOWN_ID);
    Vec3PairVec2Pair key(Vec3Pair(topLeft, bottomRight), Vec2Pair(texCoordTopLeft, texCoordBottomRight));
    VerticesIndices& vbo = registeredQuad ? _registeredQuadVBOs[id] : _quad3DTextureVBOs[key];
    
    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredQuad && vbo.first != 0) {
        Vec3PairVec2Pair& lastKey = _lastRegisteredQuad3DTexture[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderQuad() vec3 + texture VBO... RELEASING REGISTERED QUAD";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderQuad()  vec3 + texture... REUSING PREVIOUSLY REGISTERED QUAD";
        }
        #endif // def WANT_DEBUG
    }
    
    const int FLOATS_PER_VERTEX = 5; // text coords & vertices
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 4;
    const int indices = 4;
    if (vbo.first == 0) {
        _lastRegisteredQuad3DTexture[id] = key;
        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // text coords & vertices
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1, 2, 3};
        int v = 0;

        vertex[v++] = topLeft.x;
        vertex[v++] = topLeft.y;
        vertex[v++] = topLeft.z;
        vertex[v++] = texCoordTopLeft.x;
        vertex[v++] = texCoordTopLeft.y;
        
        vertex[v++] = bottomLeft.x;
        vertex[v++] = bottomLeft.y;
        vertex[v++] = bottomLeft.z;
        vertex[v++] = texCoordBottomLeft.x;
        vertex[v++] = texCoordBottomLeft.y;
        
        vertex[v++] = bottomRight.x;
        vertex[v++] = bottomRight.y;
        vertex[v++] = bottomRight.z;
        vertex[v++] = texCoordBottomRight.x;
        vertex[v++] = texCoordBottomRight.y;
        
        vertex[v++] = topRight.x;
        vertex[v++] = topRight.y;
        vertex[v++] = topRight.z;
        vertex[v++] = texCoordTopRight.x;
        vertex[v++] = texCoordTopRight.y;
        
        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;

        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "    _quad3DTextureVBOs.size():" << _quad3DTextureVBOs.size();
            } else {
                qDebug() << "new registered quad VBO made -- _registeredQuadVBOs.size():" << _registeredQuadVBOs.size();
            }
        #endif
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, NUM_BYTES_PER_VERTEX, 0);
    glTexCoordPointer(2, GL_FLOAT, NUM_BYTES_PER_VERTEX, (const void *)(3 * sizeof(float)));

    glDrawRangeElementsEXT(GL_QUADS, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderDashedLine(const glm::vec3& start, const glm::vec3& end, int id) {
    bool registered = (id != UNKNOWN_ID);
    Vec3Pair key(start, end);
    BufferDetails& details = registered ? _registeredDashedLines[id] : _dashedLines[key];

    // if this is a registered , and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registered && details.buffer.isCreated()) {
        if (_lastRegisteredDashedLines[id] != key) {
            details.buffer.destroy();
            _lastRegisteredDashedLines[id] = key;
            #ifdef WANT_DEBUG
                qDebug() << "renderDashedLine()... RELEASING REGISTERED";
            #endif // def WANT_DEBUG
        }
    }

    if (!details.buffer.isCreated()) {
    
        // draw each line segment with appropriate gaps
        const float DASH_LENGTH = 0.05f;
        const float GAP_LENGTH = 0.025f;
        const float SEGMENT_LENGTH = DASH_LENGTH + GAP_LENGTH;
        float length = glm::distance(start, end);
        float segmentCount = length / SEGMENT_LENGTH;
        int segmentCountFloor = (int)glm::floor(segmentCount);

        glm::vec3 segmentVector = (end - start) / segmentCount;
        glm::vec3 dashVector = segmentVector / SEGMENT_LENGTH * DASH_LENGTH;
        glm::vec3 gapVector = segmentVector / SEGMENT_LENGTH * GAP_LENGTH;

        const int FLOATS_PER_VERTEX = 3;
        details.vertices = (segmentCountFloor + 1) * 2;
        details.vertexSize = FLOATS_PER_VERTEX;

        GLfloat* vertexData = new GLfloat[details.vertices * FLOATS_PER_VERTEX];
        GLfloat* vertex = vertexData;

        glm::vec3 point = start;
        *(vertex++) = point.x;
        *(vertex++) = point.y;
        *(vertex++) = point.z;
        
        for (int i = 0; i < segmentCountFloor; i++) {
            point += dashVector;
            *(vertex++) = point.x;
            *(vertex++) = point.y;
            *(vertex++) = point.z;

            point += gapVector;
            *(vertex++) = point.x;
            *(vertex++) = point.y;
            *(vertex++) = point.z;
        }
        *(vertex++) = end.x;
        *(vertex++) = end.y;
        *(vertex++) = end.z;

        details.buffer.create();
        details.buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        details.buffer.bind();
        details.buffer.allocate(vertexData, details.vertices * FLOATS_PER_VERTEX * sizeof(GLfloat));
        delete[] vertexData;

        #ifdef WANT_DEBUG
        if (registered) {
            qDebug() << "new registered dashed line buffer made -- _registeredVertices:" << _registeredDashedLines.size();
        } else {
            qDebug() << "new dashed lines buffer made -- _dashedLines:" << _dashedLines.size();
        }
        #endif
    } else {
        details.buffer.bind();
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(details.vertexSize, GL_FLOAT, 0, 0);
    glDrawArrays(GL_LINES, 0, details.vertices);
    glDisableClientState(GL_VERTEX_ARRAY);
    details.buffer.release();
}

void GeometryCache::renderLine(const glm::vec3& p1, const glm::vec3& p2, int id) {
    bool registeredLine = (id != UNKNOWN_ID);
    Vec3Pair key(p1, p2);
    VerticesIndices& vbo = registeredLine ? _registeredLine3DVBOs[id] : _line3DVBOs[key];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredLine && vbo.first != 0) {
        Vec3Pair& lastKey = _lastRegisteredLine3D[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderLine() 3D ... RELEASING REGISTERED line";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderLine() 3D ... REUSING PREVIOUSLY REGISTERED line";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 3;
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 2;
    const int indices = 2;
    if (vbo.first == 0) {
        _lastRegisteredLine3D[id] = key;  

        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices, no normals because we're a 2D quad
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1};
        
        int vertexPoint = 0;

        // p1
        vertex[vertexPoint++] = p1.x;
        vertex[vertexPoint++] = p1.y;
        vertex[vertexPoint++] = p1.z;

        // p2
        vertex[vertexPoint++] = p2.x;
        vertex[vertexPoint++] = p2.y;
        vertex[vertexPoint++] = p2.z;
        

        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new renderLine() 3D VBO made -- _line3DVBOs.size():" << _line3DVBOs.size();
            } else {
                qDebug() << "new registered renderLine() 3D VBO made -- _registeredLine3DVBOs.size():" << _registeredLine3DVBOs.size();
            }
        #endif
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_LINES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GeometryCache::renderLine(const glm::vec2& p1, const glm::vec2& p2, int id) {
    bool registeredLine = (id != UNKNOWN_ID);
    Vec2Pair key(p1, p2);
    VerticesIndices& vbo = registeredLine ? _registeredLine2DVBOs[id] : _line2DVBOs[key];

    // if this is a registered quad, and we have buffers, then check to see if the geometry changed and rebuild if needed
    if (registeredLine && vbo.first != 0) {
        Vec2Pair& lastKey = _lastRegisteredLine2D[id];
        if (lastKey != key) {
            glDeleteBuffers(1, &vbo.first);
            glDeleteBuffers(1, &vbo.second);
            vbo.first = vbo.second = 0;
            #ifdef WANT_DEBUG
                qDebug() << "renderLine() 2D... RELEASING REGISTERED line";
            #endif // def WANT_DEBUG
        }
        #ifdef WANT_DEBUG
        else {
            qDebug() << "renderLine() 2D... REUSING PREVIOUSLY REGISTERED line";
        }
        #endif // def WANT_DEBUG
    }

    const int FLOATS_PER_VERTEX = 2;
    const int NUM_BYTES_PER_VERTEX = FLOATS_PER_VERTEX * sizeof(GLfloat);
    const int vertices = 2;
    const int indices = 2;
    if (vbo.first == 0) {
        _lastRegisteredLine2D[id] = key;  

        int vertexPoints = vertices * FLOATS_PER_VERTEX;
        GLfloat* vertexData = new GLfloat[vertexPoints]; // only vertices, no normals because we're a 2D quad
        GLfloat* vertex = vertexData;
        static GLubyte cannonicalIndices[indices] = {0, 1};
        
        int vertexPoint = 0;

        // p1
        vertex[vertexPoint++] = p1.x;
        vertex[vertexPoint++] = p1.y;

        // p2
        vertex[vertexPoint++] = p2.x;
        vertex[vertexPoint++] = p2.y;
        

        glGenBuffers(1, &vbo.first);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBufferData(GL_ARRAY_BUFFER, vertices * NUM_BYTES_PER_VERTEX, vertexData, GL_STATIC_DRAW);
        delete[] vertexData;
        
        GLushort* indexData = new GLushort[indices];
        GLushort* index = indexData;
        for (int i = 0; i < indices; i++) {
            index[i] = cannonicalIndices[i];
        }
        
        glGenBuffers(1, &vbo.second);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices * NUM_BYTES_PER_INDEX, indexData, GL_STATIC_DRAW);
        delete[] indexData;
        
        #ifdef WANT_DEBUG
            if (id == UNKNOWN_ID) {
                qDebug() << "new renderLine() 2D VBO made -- _line2DVBOs.size():" << _line2DVBOs.size();
            } else {
                qDebug() << "new registered renderLine() 2D VBO made -- _registeredLine2DVBOs.size():" << _registeredLine2DVBOs.size();
            }
        #endif
    
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo.first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.second);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(FLOATS_PER_VERTEX, GL_FLOAT, FLOATS_PER_VERTEX * sizeof(float), 0);
    glDrawRangeElementsEXT(GL_LINES, 0, vertices - 1, indices, GL_UNSIGNED_SHORT, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


QSharedPointer<NetworkGeometry> GeometryCache::getGeometry(const QUrl& url, const QUrl& fallback, bool delayLoad) {
    return getResource(url, fallback, delayLoad).staticCast<NetworkGeometry>();
}

QSharedPointer<Resource> GeometryCache::createResource(const QUrl& url,
        const QSharedPointer<Resource>& fallback, bool delayLoad, const void* extra) {
    QSharedPointer<NetworkGeometry> geometry(new NetworkGeometry(url, fallback.staticCast<NetworkGeometry>(), delayLoad),
        &Resource::allReferencesCleared);
    geometry->setLODParent(geometry);
    return geometry.staticCast<Resource>();
}

const float NetworkGeometry::NO_HYSTERESIS = -1.0f;

NetworkGeometry::NetworkGeometry(const QUrl& url, const QSharedPointer<NetworkGeometry>& fallback, bool delayLoad,
        const QVariantHash& mapping, const QUrl& textureBase) :
    Resource(url, delayLoad),
    _mapping(mapping),
    _textureBase(textureBase.isValid() ? textureBase : url),
    _fallback(fallback)
{
    
    if (url.isEmpty()) {
        // make the minimal amount of dummy geometry to satisfy Model
        FBXJoint joint = { false, QVector<int>(), -1 };
        _geometry.joints.append(joint);
        _geometry.leftEyeJointIndex = -1;
        _geometry.rightEyeJointIndex = -1;
        _geometry.neckJointIndex = -1;
        _geometry.rootJointIndex = -1;
        _geometry.leanJointIndex = -1;
        _geometry.headJointIndex = -1;
        _geometry.leftHandJointIndex = -1;
        _geometry.rightHandJointIndex = -1;
    }
        
    connect(this, &Resource::loaded, this, &NetworkGeometry::replaceTexturesWithPendingChanges);
}

bool NetworkGeometry::isLoadedWithTextures() const {
    if (!isLoaded()) {
        return false;
    }
    foreach (const NetworkMesh& mesh, _meshes) {
        foreach (const NetworkMeshPart& part, mesh.parts) {
            if ((part.diffuseTexture && !part.diffuseTexture->isLoaded()) ||
                    (part.normalTexture && !part.normalTexture->isLoaded()) ||
                    (part.specularTexture && !part.specularTexture->isLoaded()) ||
                    (part.emissiveTexture && !part.emissiveTexture->isLoaded())) {
                return false;
            }
        }
    }
    return true;
}

QSharedPointer<NetworkGeometry> NetworkGeometry::getLODOrFallback(float distance, float& hysteresis, bool delayLoad) const {
    if (_lodParent.data() != this) {
        return _lodParent.data()->getLODOrFallback(distance, hysteresis, delayLoad);
    }
    if (_failedToLoad && _fallback) {
        return _fallback;
    }
    QSharedPointer<NetworkGeometry> lod = _lodParent;
    float lodDistance = 0.0f;
    QMap<float, QSharedPointer<NetworkGeometry> >::const_iterator it = _lods.upperBound(distance);
    if (it != _lods.constBegin()) {
        it = it - 1;
        lod = it.value();
        lodDistance = it.key();
    }
    if (hysteresis != NO_HYSTERESIS && hysteresis != lodDistance) {
        // if we previously selected a different distance, make sure we've moved far enough to justify switching
        const float HYSTERESIS_PROPORTION = 0.1f;
        if (glm::abs(distance - qMax(hysteresis, lodDistance)) / fabsf(hysteresis - lodDistance) < HYSTERESIS_PROPORTION) {
            lod = _lodParent;
            lodDistance = 0.0f;
            it = _lods.upperBound(hysteresis);
            if (it != _lods.constBegin()) {
                it = it - 1;
                lod = it.value();
                lodDistance = it.key();
            }
        }
    }
    if (lod->isLoaded()) {
        hysteresis = lodDistance;
        return lod;
    }
    // if the ideal LOD isn't loaded, we need to make sure it's started to load, and possibly return the closest loaded one
    if (!delayLoad) {
        lod->ensureLoading();
    }
    float closestDistance = FLT_MAX;
    if (isLoaded()) {
        lod = _lodParent;
        closestDistance = distance;
    }
    for (it = _lods.constBegin(); it != _lods.constEnd(); it++) {
        float distanceToLOD = glm::abs(distance - it.key());
        if (it.value()->isLoaded() && distanceToLOD < closestDistance) {
            lod = it.value();
            closestDistance = distanceToLOD;
        }    
    }
    hysteresis = NO_HYSTERESIS;
    return lod;
}

uint qHash(const QWeakPointer<Animation>& animation, uint seed = 0) {
    return qHash(animation.data(), seed);
}

QVector<int> NetworkGeometry::getJointMappings(const AnimationPointer& animation) {
    QVector<int> mappings = _jointMappings.value(animation);
    if (mappings.isEmpty() && isLoaded() && animation && animation->isLoaded()) {
        const FBXGeometry& animationGeometry = animation->getGeometry();
        for (int i = 0; i < animationGeometry.joints.size(); i++) { 
            mappings.append(_geometry.jointIndices.value(animationGeometry.joints.at(i).name) - 1);
        }
        _jointMappings.insert(animation, mappings);
    }
    return mappings;
}

void NetworkGeometry::setLoadPriority(const QPointer<QObject>& owner, float priority) {
    Resource::setLoadPriority(owner, priority);
    
    for (int i = 0; i < _meshes.size(); i++) {
        NetworkMesh& mesh = _meshes[i];
        for (int j = 0; j < mesh.parts.size(); j++) {
            NetworkMeshPart& part = mesh.parts[j];
            if (part.diffuseTexture) {
                part.diffuseTexture->setLoadPriority(owner, priority);
            }
            if (part.normalTexture) {
                part.normalTexture->setLoadPriority(owner, priority);
            }
            if (part.specularTexture) {
                part.specularTexture->setLoadPriority(owner, priority);
            }
            if (part.emissiveTexture) {
                part.emissiveTexture->setLoadPriority(owner, priority);
            }
        }
    }
}

void NetworkGeometry::setLoadPriorities(const QHash<QPointer<QObject>, float>& priorities) {
    Resource::setLoadPriorities(priorities);
    
    for (int i = 0; i < _meshes.size(); i++) {
        NetworkMesh& mesh = _meshes[i];
        for (int j = 0; j < mesh.parts.size(); j++) {
            NetworkMeshPart& part = mesh.parts[j];
            if (part.diffuseTexture) {
                part.diffuseTexture->setLoadPriorities(priorities);
            }
            if (part.normalTexture) {
                part.normalTexture->setLoadPriorities(priorities);
            }
            if (part.specularTexture) {
                part.specularTexture->setLoadPriorities(priorities);
            }
            if (part.emissiveTexture) {
                part.emissiveTexture->setLoadPriorities(priorities);
            }
        }
    }
}

void NetworkGeometry::clearLoadPriority(const QPointer<QObject>& owner) {
    Resource::clearLoadPriority(owner);
    
    for (int i = 0; i < _meshes.size(); i++) {
        NetworkMesh& mesh = _meshes[i];
        for (int j = 0; j < mesh.parts.size(); j++) {
            NetworkMeshPart& part = mesh.parts[j];
            if (part.diffuseTexture) {
                part.diffuseTexture->clearLoadPriority(owner);
            }
            if (part.normalTexture) {
                part.normalTexture->clearLoadPriority(owner);
            }
            if (part.specularTexture) {
                part.specularTexture->clearLoadPriority(owner);
            }
            if (part.emissiveTexture) {
                part.emissiveTexture->clearLoadPriority(owner);
            }
        }
    }
}

void NetworkGeometry::setTextureWithNameToURL(const QString& name, const QUrl& url) {
    if (_meshes.size() > 0) {
        auto textureCache = DependencyManager::get<TextureCache>();
        for (int i = 0; i < _meshes.size(); i++) {
            NetworkMesh& mesh = _meshes[i];
            for (int j = 0; j < mesh.parts.size(); j++) {
                NetworkMeshPart& part = mesh.parts[j];
                
                QSharedPointer<NetworkTexture> matchingTexture = QSharedPointer<NetworkTexture>();
                if (part.diffuseTextureName == name) {
                    part.diffuseTexture =
                    textureCache->getTexture(url, DEFAULT_TEXTURE,
                                                                              _geometry.meshes[i].isEye, QByteArray());
                    part.diffuseTexture->setLoadPriorities(_loadPriorities);
                } else if (part.normalTextureName == name) {
                    part.normalTexture = textureCache->getTexture(url, DEFAULT_TEXTURE,
                                                                                                   false, QByteArray());
                    part.normalTexture->setLoadPriorities(_loadPriorities);
                } else if (part.specularTextureName == name) {
                    part.specularTexture = textureCache->getTexture(url, DEFAULT_TEXTURE,
                                                                                                     false, QByteArray());
                    part.specularTexture->setLoadPriorities(_loadPriorities);
                } else if (part.emissiveTextureName == name) {
                    part.emissiveTexture = textureCache->getTexture(url, DEFAULT_TEXTURE,
                                                                                                     false, QByteArray());
                    part.emissiveTexture->setLoadPriorities(_loadPriorities);
                }
            }
        }
    } else {
        qDebug() << "Adding a name url pair to pending" << name << url;
        // we don't have meshes downloaded yet, so hold this texture as pending
        _pendingTextureChanges.insert(name, url);
    }
}

QStringList NetworkGeometry::getTextureNames() const {
    QStringList result;
    for (int i = 0; i < _meshes.size(); i++) {
        const NetworkMesh& mesh = _meshes[i];
        for (int j = 0; j < mesh.parts.size(); j++) {
            const NetworkMeshPart& part = mesh.parts[j];
            
            if (!part.diffuseTextureName.isEmpty()) {
                QString textureURL = part.diffuseTexture->getURL().toString();
                result << part.diffuseTextureName + ":" + textureURL;
            }

            if (!part.normalTextureName.isEmpty()) {
                QString textureURL = part.normalTexture->getURL().toString();
                result << part.normalTextureName + ":" + textureURL;
            }

            if (!part.specularTextureName.isEmpty()) {
                QString textureURL = part.specularTexture->getURL().toString();
                result << part.specularTextureName + ":" + textureURL;
            }

            if (!part.emissiveTextureName.isEmpty()) {
                QString textureURL = part.emissiveTexture->getURL().toString();
                result << part.emissiveTextureName + ":" + textureURL;
            }
        }
    }
    return result;
}

void NetworkGeometry::replaceTexturesWithPendingChanges() {
    QHash<QString, QUrl>::Iterator it = _pendingTextureChanges.begin();
    
    while (it != _pendingTextureChanges.end()) {
        setTextureWithNameToURL(it.key(), it.value());
        it = _pendingTextureChanges.erase(it);
    }
}

/// Reads geometry in a worker thread.
class GeometryReader : public QRunnable {
public:

    GeometryReader(const QWeakPointer<Resource>& geometry, const QUrl& url,
        QNetworkReply* reply, const QVariantHash& mapping);

    virtual void run();

private:
     
    QWeakPointer<Resource> _geometry;
    QUrl _url;
    QNetworkReply* _reply;
    QVariantHash _mapping;
};

GeometryReader::GeometryReader(const QWeakPointer<Resource>& geometry, const QUrl& url,
        QNetworkReply* reply, const QVariantHash& mapping) :
    _geometry(geometry),
    _url(url),
    _reply(reply),
    _mapping(mapping) {
}

void GeometryReader::run() {
    QSharedPointer<Resource> geometry = _geometry.toStrongRef();
    if (geometry.isNull()) {
        _reply->deleteLater();
        return;
    }
    try {
        if (!_reply) {
            throw QString("Reply is NULL ?!");
        }
        QString urlname = _url.path().toLower();
        bool urlValid = true;
        urlValid &= !urlname.isEmpty();
        urlValid &= !_url.path().isEmpty();
        urlValid &= _url.path().toLower().endsWith(".fbx")
                    || _url.path().toLower().endsWith(".svo");

        if (urlValid) {
            // Let's read the binaries from the network
            FBXGeometry fbxgeo;
            if (_url.path().toLower().endsWith(".fbx")) {
                bool grabLightmaps = true;
                float lightmapLevel = 1.0f;
                // HACK: For monday 12/01/2014 we need to kill lighmaps loading in starchamber...
                if (_url.path().toLower().endsWith("loungev4_11-18.fbx")) {
                    grabLightmaps = false;
                } else if (_url.path().toLower().endsWith("apt8_reboot.fbx")) {
                    lightmapLevel = 4.0f;
                } else if (_url.path().toLower().endsWith("palaceoforinthilian4.fbx")) {
                    lightmapLevel = 3.5f;
                }
                fbxgeo = readFBX(_reply, _mapping, grabLightmaps, lightmapLevel);
            }
            QMetaObject::invokeMethod(geometry.data(), "setGeometry", Q_ARG(const FBXGeometry&, fbxgeo));
        } else {
            throw QString("url is invalid");
        }

    } catch (const QString& error) {
        qDebug() << "Error reading " << _url << ": " << error;
        QMetaObject::invokeMethod(geometry.data(), "finishedLoading", Q_ARG(bool, false));
    }
    _reply->deleteLater();
}

void NetworkGeometry::init() {
    _mapping = QVariantHash();
    _geometry = FBXGeometry();
    _meshes.clear();
    _lods.clear();
    _pendingTextureChanges.clear();
    _request.setUrl(_url);
    Resource::init();
}

void NetworkGeometry::downloadFinished(QNetworkReply* reply) {
    QUrl url = reply->url();
    if (url.path().toLower().endsWith(".fst")) {
        // it's a mapping file; parse it and get the mesh filename
        _mapping = readMapping(reply->readAll());
        reply->deleteLater();
        QString filename = _mapping.value("filename").toString();
        if (filename.isNull()) {
            qDebug() << "Mapping file " << url << " has no filename.";
            finishedLoading(false);
            
        } else {
            QString texdir = _mapping.value("texdir").toString();
            if (!texdir.isNull()) {
                if (!texdir.endsWith('/')) {
                    texdir += '/';
                }
                _textureBase = url.resolved(texdir);
            }
            QVariantHash lods = _mapping.value("lod").toHash();
            for (QVariantHash::const_iterator it = lods.begin(); it != lods.end(); it++) {
                QSharedPointer<NetworkGeometry> geometry(new NetworkGeometry(url.resolved(it.key()),
                    QSharedPointer<NetworkGeometry>(), true, _mapping, _textureBase));    
                geometry->setSelf(geometry.staticCast<Resource>());
                geometry->setLODParent(_lodParent);
                _lods.insert(it.value().toFloat(), geometry);
            }     
            _request.setUrl(url.resolved(filename));
            
            // make the request immediately only if we have no LODs to switch between
            _startedLoading = false;
            if (_lods.isEmpty()) {
                attemptRequest();
            }
        }
        return;
    }
    
    // send the reader off to the thread pool
    QThreadPool::globalInstance()->start(new GeometryReader(_self, url, reply, _mapping));
}

void NetworkGeometry::reinsert() {
    Resource::reinsert();
    
    _lodParent = qWeakPointerCast<NetworkGeometry, Resource>(_self);
    foreach (const QSharedPointer<NetworkGeometry>& lod, _lods) {
        lod->setLODParent(_lodParent);
    }
}

void NetworkGeometry::setGeometry(const FBXGeometry& geometry) {
    _geometry = geometry;

    auto textureCache = DependencyManager::get<TextureCache>();
    
    foreach (const FBXMesh& mesh, _geometry.meshes) {
        NetworkMesh networkMesh;
        
        int totalIndices = 0;
        foreach (const FBXMeshPart& part, mesh.parts) {
            NetworkMeshPart networkPart;
            if (!part.diffuseTexture.filename.isEmpty()) {
                networkPart.diffuseTexture = textureCache->getTexture(
                    _textureBase.resolved(QUrl(part.diffuseTexture.filename)), DEFAULT_TEXTURE,
                    mesh.isEye, part.diffuseTexture.content);
                networkPart.diffuseTextureName = part.diffuseTexture.name;
                networkPart.diffuseTexture->setLoadPriorities(_loadPriorities);
            }
            if (!part.normalTexture.filename.isEmpty()) {
                networkPart.normalTexture = textureCache->getTexture(
                    _textureBase.resolved(QUrl(part.normalTexture.filename)), NORMAL_TEXTURE,
                    false, part.normalTexture.content);
                networkPart.normalTextureName = part.normalTexture.name;
                networkPart.normalTexture->setLoadPriorities(_loadPriorities);
            }
            if (!part.specularTexture.filename.isEmpty()) {
                networkPart.specularTexture = textureCache->getTexture(
                    _textureBase.resolved(QUrl(part.specularTexture.filename)), SPECULAR_TEXTURE,
                    false, part.specularTexture.content);
                networkPart.specularTextureName = part.specularTexture.name;
                networkPart.specularTexture->setLoadPriorities(_loadPriorities);
            }
            if (!part.emissiveTexture.filename.isEmpty()) {
                networkPart.emissiveTexture = textureCache->getTexture(
                    _textureBase.resolved(QUrl(part.emissiveTexture.filename)), EMISSIVE_TEXTURE,
                    false, part.emissiveTexture.content);
                networkPart.emissiveTextureName = part.emissiveTexture.name;
                networkPart.emissiveTexture->setLoadPriorities(_loadPriorities);
            }
            networkMesh.parts.append(networkPart);
                        
            totalIndices += (part.quadIndices.size() + part.triangleIndices.size());
        }

        {
            networkMesh._indexBuffer = gpu::BufferPointer(new gpu::Buffer());
            networkMesh._indexBuffer->resize(totalIndices * sizeof(int));
            int offset = 0;
            foreach(const FBXMeshPart& part, mesh.parts) {
                networkMesh._indexBuffer->setSubData(offset, part.quadIndices.size() * sizeof(int),
                    (gpu::Resource::Byte*) part.quadIndices.constData());
                offset += part.quadIndices.size() * sizeof(int);
                networkMesh._indexBuffer->setSubData(offset, part.triangleIndices.size() * sizeof(int),
                    (gpu::Resource::Byte*) part.triangleIndices.constData());
                offset += part.triangleIndices.size() * sizeof(int);
            }
        }

        {
            networkMesh._vertexBuffer = gpu::BufferPointer(new gpu::Buffer());
            // if we don't need to do any blending, the positions/normals can be static
            if (mesh.blendshapes.isEmpty()) {
                int normalsOffset = mesh.vertices.size() * sizeof(glm::vec3);
                int tangentsOffset = normalsOffset + mesh.normals.size() * sizeof(glm::vec3);
                int colorsOffset = tangentsOffset + mesh.tangents.size() * sizeof(glm::vec3);
                int texCoordsOffset = colorsOffset + mesh.colors.size() * sizeof(glm::vec3);
                int texCoords1Offset = texCoordsOffset + mesh.texCoords.size() * sizeof(glm::vec2);
                int clusterIndicesOffset = texCoords1Offset + mesh.texCoords1.size() * sizeof(glm::vec2);
                int clusterWeightsOffset = clusterIndicesOffset + mesh.clusterIndices.size() * sizeof(glm::vec4);

                networkMesh._vertexBuffer->resize(clusterWeightsOffset + mesh.clusterWeights.size() * sizeof(glm::vec4));

                networkMesh._vertexBuffer->setSubData(0, mesh.vertices.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.vertices.constData());
                networkMesh._vertexBuffer->setSubData(normalsOffset, mesh.normals.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.normals.constData());
                networkMesh._vertexBuffer->setSubData(tangentsOffset,
                    mesh.tangents.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.tangents.constData());
                networkMesh._vertexBuffer->setSubData(colorsOffset, mesh.colors.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.colors.constData());
                networkMesh._vertexBuffer->setSubData(texCoordsOffset,
                    mesh.texCoords.size() * sizeof(glm::vec2), (gpu::Resource::Byte*) mesh.texCoords.constData());
                networkMesh._vertexBuffer->setSubData(texCoords1Offset,
                    mesh.texCoords1.size() * sizeof(glm::vec2), (gpu::Resource::Byte*) mesh.texCoords1.constData());
                networkMesh._vertexBuffer->setSubData(clusterIndicesOffset,
                    mesh.clusterIndices.size() * sizeof(glm::vec4), (gpu::Resource::Byte*) mesh.clusterIndices.constData());
                networkMesh._vertexBuffer->setSubData(clusterWeightsOffset,
                    mesh.clusterWeights.size() * sizeof(glm::vec4), (gpu::Resource::Byte*) mesh.clusterWeights.constData());

                // otherwise, at least the cluster indices/weights can be static
                networkMesh._vertexStream = gpu::BufferStreamPointer(new gpu::BufferStream());
                networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, 0, sizeof(glm::vec3));
                if (mesh.normals.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, normalsOffset, sizeof(glm::vec3));
                if (mesh.tangents.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, tangentsOffset, sizeof(glm::vec3));
                if (mesh.colors.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, colorsOffset, sizeof(glm::vec3));
                if (mesh.texCoords.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, texCoordsOffset, sizeof(glm::vec2));
                if (mesh.texCoords1.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, texCoords1Offset, sizeof(glm::vec2));
                if (mesh.clusterIndices.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, clusterIndicesOffset, sizeof(glm::vec4));
                if (mesh.clusterWeights.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, clusterWeightsOffset, sizeof(glm::vec4));

                int channelNum = 0;
                networkMesh._vertexFormat = gpu::Stream::FormatPointer(new gpu::Stream::Format());
                networkMesh._vertexFormat->setAttribute(gpu::Stream::POSITION, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
                if (mesh.normals.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::NORMAL, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
                if (mesh.tangents.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::TANGENT, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
                if (mesh.colors.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::COLOR, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::RGB));
                if (mesh.texCoords.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::TEXCOORD, channelNum++, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV));
                if (mesh.texCoords1.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::TEXCOORD1, channelNum++, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV));
                if (mesh.clusterIndices.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::SKIN_CLUSTER_INDEX, channelNum++, gpu::Element(gpu::VEC4, gpu::NFLOAT, gpu::XYZW));
                if (mesh.clusterWeights.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::SKIN_CLUSTER_WEIGHT, channelNum++, gpu::Element(gpu::VEC4, gpu::NFLOAT, gpu::XYZW));
            }
            else {
                int colorsOffset = mesh.tangents.size() * sizeof(glm::vec3);
                int texCoordsOffset = colorsOffset + mesh.colors.size() * sizeof(glm::vec3);
                int clusterIndicesOffset = texCoordsOffset + mesh.texCoords.size() * sizeof(glm::vec2);
                int clusterWeightsOffset = clusterIndicesOffset + mesh.clusterIndices.size() * sizeof(glm::vec4);

                networkMesh._vertexBuffer->resize(clusterWeightsOffset + mesh.clusterWeights.size() * sizeof(glm::vec4));
                networkMesh._vertexBuffer->setSubData(0, mesh.tangents.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.tangents.constData());
                networkMesh._vertexBuffer->setSubData(colorsOffset, mesh.colors.size() * sizeof(glm::vec3), (gpu::Resource::Byte*) mesh.colors.constData());
                networkMesh._vertexBuffer->setSubData(texCoordsOffset,
                    mesh.texCoords.size() * sizeof(glm::vec2), (gpu::Resource::Byte*) mesh.texCoords.constData());
                networkMesh._vertexBuffer->setSubData(clusterIndicesOffset,
                    mesh.clusterIndices.size() * sizeof(glm::vec4), (gpu::Resource::Byte*) mesh.clusterIndices.constData());
                networkMesh._vertexBuffer->setSubData(clusterWeightsOffset,
                    mesh.clusterWeights.size() * sizeof(glm::vec4), (gpu::Resource::Byte*) mesh.clusterWeights.constData());

                networkMesh._vertexStream = gpu::BufferStreamPointer(new gpu::BufferStream());
                if (mesh.tangents.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, 0, sizeof(glm::vec3));
                if (mesh.colors.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, colorsOffset, sizeof(glm::vec3));
                if (mesh.texCoords.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, texCoordsOffset, sizeof(glm::vec2));
                if (mesh.clusterIndices.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, clusterIndicesOffset, sizeof(glm::vec4));
                if (mesh.clusterWeights.size()) networkMesh._vertexStream->addBuffer(networkMesh._vertexBuffer, clusterWeightsOffset, sizeof(glm::vec4));

                int channelNum = 0;
                networkMesh._vertexFormat = gpu::Stream::FormatPointer(new gpu::Stream::Format());
                networkMesh._vertexFormat->setAttribute(gpu::Stream::POSITION, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
                if (mesh.normals.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::NORMAL, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
                if (mesh.tangents.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::TANGENT, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
                if (mesh.colors.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::COLOR, channelNum++, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::RGB));
                if (mesh.texCoords.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::TEXCOORD, channelNum++, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV));
                if (mesh.clusterIndices.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::SKIN_CLUSTER_INDEX, channelNum++, gpu::Element(gpu::VEC4, gpu::NFLOAT, gpu::XYZW));
                if (mesh.clusterWeights.size()) networkMesh._vertexFormat->setAttribute(gpu::Stream::SKIN_CLUSTER_WEIGHT, channelNum++, gpu::Element(gpu::VEC4, gpu::NFLOAT, gpu::XYZW));

            }
        }

        _meshes.append(networkMesh);
    }
    
    finishedLoading(true);
}

bool NetworkMeshPart::isTranslucent() const {
    return diffuseTexture && diffuseTexture->isTranslucent();
}

int NetworkMesh::getTranslucentPartCount(const FBXMesh& fbxMesh) const {
    int count = 0;
    for (int i = 0; i < parts.size(); i++) {
        if (parts.at(i).isTranslucent() || fbxMesh.parts.at(i).opacity != 1.0f) {
            count++;
        }
    }
    return count;
}
