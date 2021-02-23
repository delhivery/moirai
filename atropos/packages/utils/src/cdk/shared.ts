import { SecretValue as CdkSecretValue } from "@aws-cdk/core";
import { URL } from "url";
import { EnvironmentType, StackProps, IGitHubRepository } from "./typedefs";

export const getConstructId = (
  constructId: string,
  deployment: StackProps
): string =>
  `${deployment.environment}-${deployment.project}-${deployment.component}-${constructId}`.toLowerCase();

const getStagingBranchOrFail = (props: StackProps): string =>
  props.environment === EnvironmentType.STAGING ? "devel" : "";

const getProductionOrStagingBranch = (props: StackProps): string =>
  props.environment === EnvironmentType.PRODUCTION
    ? "main"
    : getStagingBranchOrFail(props);

export const getBranch = (props: StackProps): string =>
  getProductionOrStagingBranch(props);

export const resolveGithubRepository = (uri: URL): IGitHubRepository => {
  // const hostParts = uri.hostname.split(".");
  // const provider = hostParts[hostParts.length - 2];
  const provider = "github";
  const [, owner, repo] = uri.pathname.split("/");
  return {
    owner,
    repo,
    oauthToken: CdkSecretValue.secretsManager(`${owner}/${provider}`, {
      jsonField: "oauthToken",
    }),
  };
};
