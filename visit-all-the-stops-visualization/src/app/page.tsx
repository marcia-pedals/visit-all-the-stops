import data from "../../data/bart_viz.json"

export default function Home() {
  const latMin = 37.3;
  const latMax = 38.1;
  const lonMin = -122.5;
  const lonMax = -121.7;

  const xMax = 1200;
  const yMax = 800;

  const mapLat = (lat: number) => yMax - (lat - latMin) / (latMax - latMin) * yMax;
  const mapLon = (lon: number) => (lon - lonMin) / (lonMax - lonMin) * xMax;

  return (
    <main>
      <svg height={yMax} width={xMax}>
        {data.vertices.map((v) => {
          const x = mapLon(parseFloat(v.lon));
          const y = mapLat(parseFloat(v.lat));
          return <>
            <circle key={v.id} cx={x} cy={y} r={2} fill="black" />
            <text x={x + 4} y={y + 4} fontSize={10}>{v.name}</text>
          </>;
        })}
      </svg>
    </main>
  );
}
