#pragma once

#include "core/platform.h"
#include "math/vector.h"

namespace lambda {
    class Mesh {
        public:
            Mesh() : type(Type::TRI) {}

            static Mesh makeTri(Vector<Float, 3> * vertices, Vector<Float, 3> * normals, Vector<Float, 2> * textureCoordinates, Vector<int, 3> * faces) {
                Mesh mesh;

                mesh.type = Type::TRI;
                mesh.tri.vertices = vertices;
                mesh.tri.normals = normals;
                mesh.tri.textureCoordinates = textureCoordinates;
                mesh.tri.faces = faces;

                return mesh;
            }

            static Mesh makeQuad(Vector<Float, 3> * vertices, Vector<Float, 3> * normals, Vector<Float, 2> * textureCoordinates, Vector<int, 4> * faces) {
                Mesh mesh;

                mesh.type = Type::QUAD;
                mesh.quad.vertices = vertices;
                mesh.quad.normals = normals;
                mesh.quad.textureCoordinates = textureCoordinates;
                mesh.quad.faces = faces;

                return mesh;
            }

            LAMBDA_HOST_DEVICE bool hasNormals() const {
                switch (type) {
                    case Type::TRI: return tri.normals != nullptr;
                    case Type::QUAD: return quad.normals != nullptr;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE bool hasTextureCoordinates() const {
                switch (type) {
                    case Type::TRI: return tri.textureCoordinates != nullptr;
                    case Type::QUAD: return quad.textureCoordinates != nullptr;
                }

                return false;
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getVertex(int i, int j) const {
                switch (type) {
                    case Type::TRI: return tri.vertices ? j >= 0 && j < 3 ? tri.vertices[tri.faces[i][j]] : Vector<Float, 3>() : Vector<Float, 3>();
                    case Type::QUAD: return quad.vertices ? j >= 0 && j < 4 ? quad.vertices[quad.faces[i][j]] : Vector<Float, 3>() : Vector<Float, 3>();
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Vector<Float, 3> getNormal(int i, int j) const {
                switch (type) {
                    case Type::TRI: return tri.normals ? j >= 0 && j < 3 ? tri.normals[tri.faces[i][j]] : Vector<Float, 3>() : Vector<Float, 3>();
                    case Type::QUAD: return quad.normals ? j >= 0 && j < 4 ? quad.normals[quad.faces[i][j]] : Vector<Float, 3>() : Vector<Float, 3>();
                }

                return Vector<Float, 3>();
            }

            LAMBDA_HOST_DEVICE Vector<Float, 2> getTextureCoordinate(int i, int j) const {
                switch (type) {
                    case Type::TRI: return tri.textureCoordinates ? j >= 0 && j < 3 ? tri.textureCoordinates[tri.faces[i][j]] : Vector<Float, 2>() : Vector<Float, 2>();
                    case Type::QUAD: return quad.textureCoordinates ? j >= 0 && j < 4 ? quad.textureCoordinates[quad.faces[i][j]] : Vector<Float, 2>() : Vector<Float, 2>();
                }

                return Vector<Float, 2>();
            }

        private:
            enum class Type { TRI, QUAD };

            Type type;

            union {
                struct { Vector<Float, 3> * vertices, * normals; Vector<Float, 2> * textureCoordinates; Vector<int, 3> * faces; } tri;
                struct { Vector<Float, 3> * vertices, * normals; Vector<Float, 2> * textureCoordinates; Vector<int, 4> * faces; } quad;
            };
    };
}