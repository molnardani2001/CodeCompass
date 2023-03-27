using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace CSharpParser.Migrations
{
    public partial class CsharpEdge_Migration : Migration 
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "CsharpEdges",
                columns: table => new
                {
                    Id = table.Column<decimal>(type: "numeric(20,0)", nullable: false),
                    From = table.Column<decimal>(type: "numeric(20,0)", nullable: false),
                    To = table.Column<decimal>(type: "numeric(20,0)", nullable: false),
                    Type = table.Column<int>(type: "integer", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_CsharpEdges", x => x.Id);
                });
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "CsharpEdges");
        }
    }
}
