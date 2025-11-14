import { useEffect } from "react";
import { useNavigate } from "react-router-dom";

export default function Auth() {
  const nav = useNavigate();
  useEffect(() => {
    const hash = window.location.hash.substring(1);
    const params = new URLSearchParams(hash);
    const token = params.get("access_token");
    if (token) {
      localStorage.setItem("gh_token", token);
      nav("/viewer");
    } else {
      nav("/");
    }
  }, []);
  return <p>Authorizing...</p>;
}
