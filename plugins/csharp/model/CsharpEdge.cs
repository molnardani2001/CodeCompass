using System.ComponentModel.DataAnnotations.Schema;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace CSharpParser.model
{
    class CsharpEdge 
    {
        enum Type 
        {
            PROVIDE,
            IMPLEMENT,
            USE,
            DEPEND
        }

        public ulong Id { get; set; }
        public ulong From { get; set; }
        public ulong To { get; set; }
        public Type Type { get; set; }

        public string typeToString(CsharpEdge::Type type_)
        {
            switch (type_)
            {
                case CsharpEdge.Type.PROVIDE: return "Provide";
                case CsharpEdge.Type.IMPLEMENT: return "Implement";
                case CsharpEdge.Type.USE: return "Use";
                case CsharpEdge.Type.DEPEND: return "Depend";
            }

            return string.Empty;
        }

        public ulong createIdentifier(CsharpEdge edge_)
        {
            return (edge_.From.ToString() + 
                    edge_.To.ToString() +
                    typeToString(edge_.Type));
        }
    }
}