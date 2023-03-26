using System.Text;
using Microsoft.EntityFrameworkCore;

namespace CSharpParser.model
{
    class CsharpDbContext : DbContext
    {
        private readonly bool isMigration = false;
        public CsharpDbContext()
        {
            isMigration = true;
        }

        protected override void OnConfiguring(DbContextOptionsBuilder optionsBuilder)
        {
            if (isMigration)
            {
                optionsBuilder.UseNpgsql(@"Server=localhost;Port=5432;Database=migrations;User Id=compass;Password=compass");
                base.OnConfiguring(optionsBuilder);
            }
        }

        public CsharpDbContext(DbContextOptions<CsharpDbContext> options) : base(options) { }
        
        public DbSet<CsharpAstNode> CsharpAstNodes { get; set; }
        public DbSet<CsharpNamespace> CsharpNamespaces { get; set; }
        public DbSet<CsharpClass> CsharpClasses { get; set; }
        public DbSet<CsharpMethod> CsharpMethods { get; set; }
        public DbSet<CsharpVariable> CsharpVariables { get; set; }
        public DbSet<CsharpStruct> CsharpStructs { get; set; }
        public DbSet<CsharpEnum> CsharpEnums { get; set; }
        public DbSet<CsharpEnumMember> CsharpEnumMembers { get; set; }        
        public DbSet<CsharpEtcEntity> CsharpEtcEntitys { get; set; }
        public DbSet<CsharpEdge> CsharpEdges { get; set; }
    }

}
