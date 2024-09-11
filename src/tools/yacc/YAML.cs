using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace YACC
{
    internal class YAML
    {
        public static IDeserializer YamlDeserializer
        {
            get => new DeserializerBuilder()
                .WithNamingConvention(CamelCaseNamingConvention.Instance)
                .Build();
        }
    }
}
