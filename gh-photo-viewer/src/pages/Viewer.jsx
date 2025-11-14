import { useEffect, useState } from "react";

export default function Viewer() {
  const [files, setFiles] = useState([]);
  const [owner, setOwner] = useState("");
  const [repo, setRepo] = useState("");
  const [loading, setLoading] = useState(false);
  const token = localStorage.getItem("gh_token");

  async function fetchFiles() {
    if (!owner || !repo) return;
    setLoading(true);
    const res = await fetch(
      `https://api.github.com/repos/${owner}/${repo}/contents/track-my-photo`,
      { headers: { Authorization: `token ${token}` } }
    );
    const data = await res.json();
    setFiles(Array.isArray(data) ? data : []);
    setLoading(false);
  }

  return (
    <div style={{ padding: 20 }}>
      <h2>Photo Viewer</h2>
      <div>
        <input
          placeholder="owner"
          value={owner}
          onChange={(e) => setOwner(e.target.value)}
        />
        <input
          placeholder="repo"
          value={repo}
          onChange={(e) => setRepo(e.target.value)}
        />
        <button onClick={fetchFiles}>Load</button>
      </div>
      {loading && <p>Loading...</p>}
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(auto-fill, 100px)",
          gap: 4,
        }}
      >
        {files.map((f) => (
          <div key={f.path}>
            <img
              src={f.download_url}
              style={{ width: 100, height: 100, objectFit: "cover" }}
            />
            <small>{f.name}</small>
          </div>
        ))}
      </div>
    </div>
  );
}
