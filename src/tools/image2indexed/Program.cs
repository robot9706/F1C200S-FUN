using System.Drawing;
using System.Linq;
using System.Text;

namespace image2indexed
{
    internal class Program
    {
        static void Main(string[] args)
        {
            string image = args[0];
            string paletteImageFile = args[1];
            string output = args[2];

            Color[] palette;
            using (Bitmap paletteImage = (Bitmap)Bitmap.FromFile(paletteImageFile))
            {
                int entries = paletteImage.Width * paletteImage.Height;
                palette = new Color[entries];

                int idx = 0;
                for (int y = 0; y < paletteImage.Height; y++)
                {
                    for (int x = 0; x < paletteImage.Width; x++)
                    {
                        palette[idx] = paletteImage.GetPixel(x, y);
                        idx++;
                    }
                }
            }

            using (Bitmap inputImage = (Bitmap)Bitmap.FromFile(image))
            {
                string outExt = Path.GetExtension(output).ToLowerInvariant();
                switch (outExt)
                {
                    case ".bin":
                        using (FileStream outFile = new FileStream(output, FileMode.Create))
                        {
                            for (int y = 0; y < inputImage.Height; y++)
                            {
                                for (int x = 0; x < inputImage.Width; x++)
                                {
                                    Color color = inputImage.GetPixel(x, y);
                                    int index = Array.IndexOf(palette, color);

                                    if (index < 0)
                                    {
                                        throw new Exception($"Unknown color: {color}");
                                    }

                                    outFile.WriteByte((byte)index);
                                }
                            }
                        }
                        break;

                    case ".h":
                        using (StreamWriter writer = new StreamWriter(output, false, Encoding.UTF8))
                        {
                            writer.WriteLine("static uint8_t image[] = {");
                            for (int y = 0; y < inputImage.Height; y++)
                            {
                                for (int x = 0; x < inputImage.Width; x++)
                                {
                                    Color color = inputImage.GetPixel(x, y);
                                    int index = Array.IndexOf(palette, color);

                                    if (index < 0)
                                    {
                                        throw new Exception($"Unknown color: {color}");
                                    }

                                    writer.Write($"0x{(index.ToString("X2"))}, ");
                                }
                                writer.WriteLine();
                            }
                            writer.WriteLine("};");
                        }
                        break;
                }
            }
        }
    }
}
