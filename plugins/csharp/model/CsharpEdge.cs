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
    }
}