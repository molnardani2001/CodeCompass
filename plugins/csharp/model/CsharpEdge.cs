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
    }
}