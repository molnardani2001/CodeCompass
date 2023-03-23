using System;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace CSharpParser.model
{
    enum EdgeType 
    {
        PROVIDE,
        IMPLEMENT,
        USE,
        DEPEND
    }
    class CsharpEdge 
    {
        public ulong Id { get; set; }
        public ulong From { get; set; }
        public ulong To { get; set; }
        public EdgeType Type { get; set; }

        public string typeToString(EdgeType type_)
        {
            switch (type_)
            {
                case EdgeType.PROVIDE: return "Provide";
                case EdgeType.IMPLEMENT: return "Implement";
                case EdgeType.USE: return "Use";
                case EdgeType.DEPEND: return "Depend";
            }

            return string.Empty;
        }

        public ulong createIdentifier(CsharpEdge edge_)
        {
            return fnvHash($"{edge_.From}{edge_.To}{typeToString(edge_.Type)}");
        }

        private ulong fnvHash(string data_)
        {
            ulong hash = 16695981039346656037;

            int len = data_.Length;
            for (int i = 0; i < len; ++i)
            {
                hash ^= data_[i];
                hash *= 1699511628211;
            }

            return hash;
        }     
    }
}