using System;
using System.Collections.Generic;
using System.Linq;
using static System.Console;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.FindSymbols;
using CSharpParser.model;
using Microsoft.CodeAnalysis;
using System.Threading.Tasks;
using System.Collections.Concurrent;

namespace CSharpParser
{
    partial class RelationCollector : CSharpSyntaxWalker
    {
        private readonly CsharpDbContext DbContext;
        private readonly SemanticModel Model;
        private readonly SyntaxTree Tree;
        private readonly ConcurrentDictionary<ulong, CsharpEdge> Edges;

        public RelationCollector(CsharpDbContext context, SemanticModel model, SyntaxTree tree, ConcurrentDictionary<ulong, CsharpEdge> edges)
        {
            this.DbContext = context;
            this.Model = model;
            this.Tree = tree;  
            this.Edges = edges;       
        }    

        /// <summary>
        /// Method <c>VisitVariableDeclarator</c> will gather aggregation type of relations.
        /// </summary>
        public override void VisitVariableDeclarator(VariableDeclaratorSyntax node)
        {
            if (node.Initializer == null) return;

            var symbol = Model.GetSymbolInfo(node.Initializer.Value).Symbol;
            if (symbol == null || symbol.ContainingAssembly.Identity != Model.Compilation.Assembly.Identity) return;

            var symbolLocation = symbol.Locations.FirstOrDefault(loc => loc.IsInSource);
            if (symbolLocation == null) return;

            var symbolFilePath = symbolLocation.SourceTree?.FilePath;
            var usageFilePath = node.SyntaxTree.FilePath;
            if (symbolFilePath == null || 
                usageFilePath == null ||
                symbolFilePath == usageFilePath) return;


            // WriteLine($"Value usageFilePath: {usageFilePath}; symbolFilePath: {symbolFilePath}");

            CsharpEdge csharpEdge = new CsharpEdge();
            csharpEdge.From = fnvHash(usageFilePath);
            csharpEdge.To = fnvHash(symbolFilePath);
            csharpEdge.Type = EdgeType.USE;
            csharpEdge.Id = createIdentifier(csharpEdge);
            
            Edges.TryAdd(csharpEdge.Id,csharpEdge);

            base.VisitVariableDeclarator(node);
        }

        /// <summary>
        /// Method <c>VisitInvocationExpression</c> will gather association type of relations.
        /// </summary>
        public override void VisitInvocationExpression(InvocationExpressionSyntax node)
        {
            var methodSymbol = Model.GetSymbolInfo(node).Symbol as IMethodSymbol;
             if (methodSymbol == null || 
                 methodSymbol.ContainingAssembly.Identity != Model.Compilation.Assembly.Identity) return;

            var symbolFilePath = methodSymbol.Locations.FirstOrDefault(loc => loc.IsInSource)?.SourceTree?.FilePath;
            var usageFilePath = node.SyntaxTree.FilePath;
            if (symbolFilePath == null || 
                usageFilePath == null ||
                symbolFilePath == usageFilePath) return;

            // WriteLine($"Value usageFilePath: {usageFilePath}; symbolFilePath: {symbolFilePath}");

            CsharpEdge csharpEdge = new CsharpEdge();
            csharpEdge.From = fnvHash(usageFilePath);
            csharpEdge.To = fnvHash(symbolFilePath);
            csharpEdge.Type = EdgeType.USE;
            csharpEdge.Id = createIdentifier(csharpEdge);
            
            Edges.TryAdd(csharpEdge.Id,csharpEdge);
            
            base.VisitInvocationExpression(node);
        }

        private ulong createIdentifier(CsharpEdge edge_)
        {
            return fnvHash($"{edge_.From}{edge_.To}{typeToString(edge_.Type)}");
        }

        private ulong fnvHash(string data_)
        {
            ulong hash = 14695981039346656037;

            int len = data_.Length;
            for (int i = 0; i < len; ++i)
            {
                hash ^= data_[i];
                hash *= 1099511628211;
            }

            return hash;
        }

        private string typeToString(EdgeType type_)
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