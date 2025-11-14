export default function App() {
  const CLIENT_ID = "Iv23lioRCAXLuSsgeirI";
  const REDIRECT = document.location;
  const login = () => {
    const url = `https://github.com/login/oauth/authorize?client_id=${CLIENT_ID}&scope=repo`;
    window.location.href = url;
  };

  return (
    <div style={{ textAlign: "center", padding: 50 }}>
      <h2>ESP32 Photo Viewer</h2>
      <p>Log in with GitHub to view uploaded photos.</p>
      <button onClick={login}>Login with GitHub</button>
    </div>
  );
}
