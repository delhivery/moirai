import { Construct, SecretValue, StackProps } from "@aws-cdk/core";
import { URL } from "url";

enum EnvironmentType {
  PRODUCTION = "production",
  STAGING = "staging",
  DEVELOPER = "developer",
  LOCAL = "local",
  NULL = "null",
}

export interface IBaseProps extends StackProps {
  project: string;
  component: string;
  technologyUnit: string;
  environment: EnvironmentType;
  uri: URL;
}

export interface IDeployment {
  // Project
  prj: string;
  // Component
  cmp: string;
  // TechnologyUnit
  tun: string;
  // Environment
  env: string;
}

interface IGitHubRepository {
  owner: string;
  repo: string;
  oauthToken: SecretValue;
}

export const getConstructId = (
  constructId: string,
  resource: Construct,
  deployment: IDeployment
): string =>
  `${deployment.env}-${deployment.prj}.${
    deployment.cmp
  }-${resource.toString()}-${constructId}`;

const getStagingBranchOrFail = (props: IBaseProps): string =>
  props.environment === EnvironmentType.STAGING ? "devel" : "";

const getProductionOrStagingBranch = (props: IBaseProps): string =>
  props.environment === EnvironmentType.PRODUCTION
    ? "main"
    : getStagingBranchOrFail(props);

export const getBranch = (props: IBaseProps): string =>
  getProductionOrStagingBranch(props);

export const resolveGithubRepository = (uri: URL): IGitHubRepository => {
  // const hostParts = uri.hostname.split(".");
  // const provider = hostParts[hostParts.length - 2];
  const provider = "github";
  const [, owner, repo] = uri.pathname.split("/");
  return {
    owner,
    repo,
    oauthToken: SecretValue.secretsManager(`${owner}/${provider}`, {
      jsonField: "oauthToken",
    }),
  };
};
